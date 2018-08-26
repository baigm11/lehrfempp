#ifndef _LF_REFINEMENT_HIER_H_
#define _LF_REFINEMENT_HIER_H_

/**
 * @file refinement_hierarchy.h
 * @brief Data structures for managing nested meshes created by refinement
 *
 */

#include "hybrid2d_refinement_pattern.h"

namespace lf::refinement {

/**
 * @brief Information about the refinement status of a point
 *
 * This information is used by methods of the class `MeshHierarchy` to
 * organize uniform and adaptive refinement.
 *
 */
struct PointChildInfo {
  explicit PointChildInfo() = default;
  /** @brief a flag indicating whether the point is to be duplicated (rp_copy)
   */
  RefPat ref_pat{RefPat::rp_nil};
  /** @brief global index of the child point */
  glb_idx_t child_point_idx{idx_nil};
};

/**
 * @brief Information about the refinement status of an edge
 *
 * This information is used by methods of the class `MeshHierarchy` to
 * organize uniform and adaptive refinement.
 *
 * The key pieces of information are
 * - the topological refinement type (rp_copy or rp_split)
 * - indices of all _interior_ child entities (edges or points)
 */
struct EdgeChildInfo {
  explicit EdgeChildInfo() = default;
  /** @brief type of refinement edge has undergone, see `RefPat` */
  RefPat ref_pat_{RefPat::rp_nil};
  /** @brief global indices of child edges in fine mesh */
  std::vector<glb_idx_t> child_edge_idx;
  /** @brief global indices of _interior_ child points in fine mesh */
  std::vector<glb_idx_t> child_point_idx;
};

/**
 * @brief Information about the refinement status of a cell
 *
 * This information is used by methods of the class `MeshHierarchy` to
 * organize uniform and adaptive refinement.
 *
 * The key pieces of information are
 * - the topological refinement type along with the anchor index used
 *   in the case of a cell, which completely defines the geometric refinement.
 * - indices of all _interior_ child entities
 */
struct CellChildInfo {
  explicit CellChildInfo() = default;
  RefPat ref_pat_{RefPat::rp_nil};
  sub_idx_t anchor_{idx_nil};
  std::vector<glb_idx_t> child_cell_idx;
  std::vector<glb_idx_t> child_edge_idx;
  std::vector<glb_idx_t> child_point_idx;
};

/**
 * @brief Information about possible parent entities
 */
struct ParentInfo {
  explicit ParentInfo() = default;
  // Data members
  const mesh::Entity *parent_ptr{
      nullptr}; /**< parent entity, not necessarily the same type */
  glb_idx_t parent_index{
      idx_nil}; /**< index of parent entity w.r.t. coarse mesh */
  sub_idx_t child_number{idx_nil}; /**< local index in the parent entity */
};

/**
 * @brief A hierarchy of nested 2D hybrid meshes created by refinement
 */
class MeshHierarchy {
 public:
  /**
   * @brief Initialize mesh hierarchy with an existing coarsest mesh
   *
   * @param base_mesh valid pointer to coarsest mesh
   * @param mesh_factory factory object creating new meshes during refinement
   */
  MeshHierarchy(std::shared_ptr<mesh::Mesh> base_mesh,
                std::shared_ptr<mesh::MeshFactory> mesh_factory);
  MeshHierarchy(const MeshHierarchy &) = delete;
  MeshHierarchy &operator=(const MeshHierarchy &) = delete;
  MeshHierarchy(MeshHierarchy &&) = delete;
  MeshHierarchy &operator=(MeshHierarchy &&) = delete;

  /**
   * @brief number of meshes contained in the hierarchy, 1 for a single mesh
   */
  size_type NumLevels() const { return meshes_.size(); }

  /**
   * @brief access the mesh on a particular level
   */
  std::shared_ptr<const mesh::Mesh> getMesh(size_type level) const {
    LF_VERIFY_MSG(level < meshes_.size(),
                  "Level " << level << " outside scope");
    return meshes_.at(level);
  }
  std::shared_ptr<mesh::Mesh> getMesh(size_type level) {
    LF_VERIFY_MSG(level < meshes_.size(),
                  "Level " << level << " outside scope");
    return meshes_.at(level);
  }

  /**
   * @brief Obtain refinement information for all points
   */
  const std::vector<PointChildInfo> &PointChildInfos(size_type level) const {
    LF_VERIFY_MSG(level < NumLevels(), "Illegal level " << level);
    return point_child_infos_[level];
  }
  /**
   * @brief Obtain refinement information for all edges
   */
  const std::vector<EdgeChildInfo> &EdgeChildInfos(size_type level) const {
    LF_VERIFY_MSG(level < NumLevels(), "Illegal level " << level);
    return edge_child_infos_[level];
  }
  /**
   e* @brief Obtain refinement information for all
   */
  const std::vector<CellChildInfo> &CellChildInfos(size_type level) const {
    LF_VERIFY_MSG(level < NumLevels(), "Illegal level " << level);
    return cell_child_infos_[level];
  }
  /**
   * @brief Fetch information about parents
   */
  const std::vector<ParentInfo> &ParentInfos(size_type level,
                                             dim_t codim) const {
    LF_VERIFY_MSG(level < NumLevels(), "Illegal level " << level);
    LF_VERIFY_MSG(codim < 3, "Codim = " << codim << " illegal");
    return parent_infos_[level][codim];
  }
  /**
   * @brief Access refinement edge indices
   */
  const std::vector<glb_idx_t> &RefinementEdges(size_type level) const {
    LF_VERIFY_MSG(level < NumLevels(), "Illegal level " << level);
    return refinement_edges_[level];
  }

  /**
   * @brief Perform regular or barycentric uniform refinement of the finest mesh
   * in the hierarchy
   *
   * @param ref_pat selector for type of uniform refinement: default is
   * rp_regular, rp_barycentric choses barycentric refinement.
   *
   * This method carries out uniform refinement of all cells of a mesh according
   * to the `rp_regular` or `rp_barycentric` refinement patterns.
   *
   * Regular refinement means that every node is copied, every edge is split
   * and every cell is subdivided into four or six smaller ones of the same
   * shape.
   */
  void RefineRegular(RefPat ref_pat = RefPat::rp_regular);
  /**
   * @brief Mark the edges of a mesh based on a predicate
   *
   * @param marker this should be functor of type
   * `std::function<bool(const Mesh &,const Entity &)>`
   * returning true if the passed edge is to be marked.
   *
   * The _marker_ object also takes a reference to a mesh, because
   * marking makes sense only for the finest level. The mesh on the
   * finest level is provided to the marker object by the `MeshHierarchy`.
   */
  template <typename Marker>
  void MarkEdges(Marker &&marker);

  /**
   * @brief Conduct local refinement of the mesh splitting all marked edges
   */
  void RefineMarked();
  /**
   * @brief Destroy the mesh on the finest level unless it is the base mesh
   */
  void Coarsen();

  virtual ~MeshHierarchy() = default;

 private:
  /**
   * @brief Create new mesh according to refinement pattern
   *        provided for entities
   *
   * This method assumes that a refinement pattern has already be set
   * in the `ChildInfo` structure for each entity. According to this
   * information, refinement is carried out.
   */
  void PerformRefinement();

 private:
  /** the meshes managed by the MeshHierarchy object */
  std::vector<std::shared_ptr<mesh::Mesh>> meshes_;
  /** The mesh factory to be used to creating a new mesh */
  std::shared_ptr<mesh::MeshFactory> mesh_factory_;
  /** information about children for each level and each class of entities */
  std::vector<std::vector<PointChildInfo>> point_child_infos_;
  std::vector<std::vector<EdgeChildInfo>> edge_child_infos_;
  std::vector<std::vector<CellChildInfo>> cell_child_infos_;
  /** information about parent entities on each level */
  std::vector<std::array<std::vector<ParentInfo>, 3>> parent_infos_;
  /** Information about marked edges */
  std::vector<std::vector<bool>> edge_marked_;
  /** Information about local refinement edges of triangles */
  std::vector<std::vector<sub_idx_t>> refinement_edges_;

  /**
   * @brief Finds the index of the longest edge of a triangle
   */
  sub_idx_t LongestEdge(const lf::mesh::Entity &T) const;

 public:
  /** @brief diagnostics control variable */
  static int output_ctrl_;
};

template <typename Marker>
void MeshHierarchy::MarkEdges(Marker &&marker) {
  // Retrieve the finest mesh in the hierarchy
  const mesh::Mesh &finest_mesh(*meshes_.back());

  LF_VERIFY_MSG(edge_marked_.back().size() == finest_mesh.Size(1),
                "Length  mismatch for edge flag array");

  // Run through the edges = entities of co-dimension 1
  for (const mesh::Entity &edge : finest_mesh.Entities(1)) {
    glb_idx_t edge_index = finest_mesh.Index(edge);
    (edge_marked_.back())[edge_index] = marker(finest_mesh, edge);
  }
}

}  // namespace lf::refinement

#endif