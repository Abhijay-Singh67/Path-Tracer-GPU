#ifndef BVH_H
#define BVH_H

#include "aabb.h"
#include "structs.h"
#include <iostream>
#include <vector>
#include <algorithm>

struct PrimitiveRef {
    int primitive_type; //0 = sphere, 1 = quad, 2 = triangle
    int index; // index in the SSBO
    aabb bounds;
    glm::vec4 centroid;
};

class bvh_node{
    public:
        static constexpr int LEAF_THRESHOLD = 4;
        static constexpr int NUM_BINS = 16;
        static constexpr float TRAVERSAL_COST = 1.0f;
        static constexpr float INTERSECT_COST = 1.0f;

        bvh_node(std::vector<PrimitiveRef> &refs, int start, int end){
            //Make the bounding box of the current node
            bbox = aabb::empty;
            for (int i = start; i < end; i++){
                bbox = aabb(bbox, refs[i].bounds);
            }

            int n = end - start;

            if(n <= LEAF_THRESHOLD){
                make_leaf(start, n);
                return;
            }

            //Compute Centroid Bounds. We bin over these, not the full primitives
            aabb centroid_bounds = aabb::empty;
            for (int i = start; i < end; i++) {
                centroid_bounds = aabb(centroid_bounds, refs[i].centroid);
            }

            //Now we choose the best axis for SAH
            int best_axis = -1;
            int best_bin = -1;
            float best_cost = std::numeric_limits<float>::infinity();

            for(int axis = 0; axis < 3; axis++){
                const interval& ci = centroid_bounds.axis_interval(axis);
                float ext = float(ci.size());
                if (ext <= 0.0f) continue; // all centroids identical along the axis

                struct Bin { aabb bounds = aabb::empty; int count = 0; };
                Bin bins[NUM_BINS];

                float inv_ext = float(NUM_BINS) / ext;
                float origin = float(ci.min);

                //Bucket primitives into the bins
                for (int i = start; i < end; i++){
                    int b = int((refs[i].centroid[axis] - origin) * inv_ext);
                    if(b == NUM_BINS) b = NUM_BINS - 1;
                    if (b < 0) b = 0;
                    bins[b].bounds = aabb(bins[b].bounds, refs[i].bounds);
                    bins[b].count += 1;
                }

                // Prefix sums (left side: bins [0..i])
                aabb left_bounds[NUM_BINS - 1];
                int  left_counts[NUM_BINS - 1];
                aabb running = aabb::empty;
                int  rc = 0;
                for (int i = 0; i < NUM_BINS - 1; i++) {
                    running = aabb(running, bins[i].bounds);
                    rc += bins[i].count;
                    left_bounds[i] = running;
                    left_counts[i] = rc;
                }

                // Suffix sums (right side: bins [i+1..NUM_BINS-1])
                aabb right_bounds[NUM_BINS - 1];
                int  right_counts[NUM_BINS - 1];
                running = aabb::empty;
                rc = 0;
                for (int i = NUM_BINS - 1; i >= 1; i--) {
                    running = aabb(running, bins[i].bounds);
                    rc += bins[i].count;
                    right_bounds[i - 1] = running;
                    right_counts[i - 1] = rc;
                }

                float parent_sa = bbox.surface_area();

                for (int i = 0; i < NUM_BINS - 1; i++) {
                    if (left_counts[i] == 0 || right_counts[i] == 0) continue;

                    float cost = TRAVERSAL_COST
                        + (left_bounds[i].surface_area()  / parent_sa) * left_counts[i]  * INTERSECT_COST
                        + (right_bounds[i].surface_area() / parent_sa) * right_counts[i] * INTERSECT_COST;

                    if (cost < best_cost) {
                        best_cost = cost;
                        best_axis = axis;
                        best_bin  = i;
                    }
                }
            }

            float leaf_cost = n * INTERSECT_COST;
            if (best_axis < 0 || best_cost >= leaf_cost) {
                make_leaf(start, n);
                return;
            }

            // Partition refs based on chosen split
            const interval& ci = centroid_bounds.axis_interval(best_axis);
            float ext      = float(ci.size());
            float inv_ext  = float(NUM_BINS) / ext;
            float origin   = float(ci.min);
            int   split_bin = best_bin;
            int   ax        = best_axis;

            auto pivot = std::partition(refs.begin() + start, refs.begin() + end,
                [ax, origin, inv_ext, split_bin](const PrimitiveRef& r) {
                    int b = int((r.centroid[ax] - origin) * inv_ext);
                    if (b == NUM_BINS) b = NUM_BINS - 1;
                    if (b < 0) b = 0;
                    return b <= split_bin;
                });

            int mid = int(pivot - refs.begin());

            if (mid == start || mid == end) {
                make_leaf(start, n);
                return;
            }

            left  = new bvh_node(refs, start, mid);
            right = new bvh_node(refs, mid, end);
            ref_count = 0;
            first_ref = -1;
        }

        ~bvh_node() {
            delete left;
            delete right;
        }

        bvh_node(const bvh_node&) = delete;
        bvh_node& operator=(const bvh_node&) = delete;

        bool is_leaf() const { return ref_count > 0; }
        aabb bounds() const { return bbox; }
        bvh_node* left_child()  const { return left; }
        bvh_node* right_child() const { return right; }
        int leaf_first_ref()    const { return first_ref; }
        int leaf_ref_count()    const { return ref_count; }

                // Flattens this BVH into a flat array of GPUBVHNode for SSBO upload.
        // Returns the array; the root is always at index 0.
        std::vector<GPUBVHNode> flatten() const {
            std::vector<GPUBVHNode> nodes;
            nodes.reserve(count_nodes());  // optional pre-allocation
            flatten_recursive(nodes);
            return nodes;
        }

        // Returns the total number of nodes in the tree (useful for diagnostics)
        int count_nodes() const {
            if (is_leaf()) return 1;
            return 1 + left->count_nodes() + right->count_nodes();
        }

        // Returns max depth of the tree (useful for setting GPU stack size)
        int max_depth() const {
            if (is_leaf()) return 1;
            return 1 + std::max(left->max_depth(), right->max_depth());
        }
    
    private:
        aabb bbox;
        bvh_node* left = nullptr;
        bvh_node* right = nullptr;
        int first_ref = -1;
        int ref_count = 0;

        void make_leaf(int start, int count) {
            first_ref = start;
            ref_count = count;
            left = right = nullptr;
        }

        // Recursive flatten worker. Appends this node to `nodes`, then its children.
        // Returns the index where this node was placed in `nodes`.
        int flatten_recursive(std::vector<GPUBVHNode>& nodes) const {
            int my_index = (int)nodes.size();
            nodes.push_back(GPUBVHNode{});  // reserve slot first; we'll fill it after recursing

            GPUBVHNode node;
            node.bbox_min = glm::vec4(float(bbox.x.min), float(bbox.y.min), float(bbox.z.min), 0.0f);
            node.bbox_max = glm::vec4(float(bbox.x.max), float(bbox.y.max), float(bbox.z.max), 0.0f);

            if (is_leaf()) {
                node.data = glm::ivec4(first_ref, ref_count, 1, 0);
                nodes[my_index] = node;
            } else {
                // Recurse left first, then right. Record their flat indices.
                int left_idx  = left->flatten_recursive(nodes);
                int right_idx = right->flatten_recursive(nodes);
                node.data = glm::ivec4(left_idx, right_idx, 0, 0);
                nodes[my_index] = node;
            }

            return my_index;
        }
};

#endif