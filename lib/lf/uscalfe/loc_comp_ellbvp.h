#ifndef LF_LOCCOMPELLBVP
#define LF_LOCCOMPELLBVP
/***************************************************************************
 * LehrFEM++ - A simple C++ finite element libray for teaching
 * Developed from 2018 at the Seminar of Applied Mathematics of ETH Zurich,
 * lead developers Dr. R. Casagrande and Prof. R. Hiptmair
 ***************************************************************************/

/**
 * @file
 * @brief Classes taking care of local computations for scalar 2nd-order
 * elliptic BVPs
 * @author Ralf Hiptmair
 * @date October 2018
 * @copyright MIT License
 */

#include <lf/quad/quad.h>
#include <iostream>
#include "lagr_fe.h"
#include "mesh_function_traits.h"
#include "precomputed_scalar_reference_finite_element.h"

namespace lf::uscalfe {

/**
 * @brief Class for local quadrature based computations for Lagrangian finite
 * elements and second-order scalar elliptic BVPs.
 *
 * @tparam SCALAR type for the entries of the element matrices. Must be a field
 *                     type such as `double` or `std::complex<double>`
 * @tparam DIFF_COEFF a \ref mesh_function "MeshFunction" that defines the
 *                    diffusion coefficient \f$ \mathbf{\alpha} \f$.
 *                    It should be either scalar- or matrix-valued.
 * @tparam REACTION_COEFF a \ref mesh_function "MeshFunction" that defines the
 *                        reaction coefficient \f$ \mathbf{\gamma} \f$.
 *                    It should be either scalar- or matrix-valued.
  *
 * @note This class complies with the type requirements for the template
 * argument ELEM_MAT_COMP of the function lf::assemble::AssembleMatrixLocally().
 *
 * The element matrix is corresponds to the (local) bilinear form
 * @f[
    (u,v) \mapsto\int\limits_{K}\boldsymbol{\alpha}(\mathbf{x})\mathbf{grad}\,u
          \cdot\mathbf{grad}\,v + \gamma(\mathbf{x})u\,v\,\mathrm{d}\mathbf{x}
 \;,
 * @f]
 * with _diffusion coefficient_ @f$\mathbf{\alpha}@f$ and reaction coefficient
 * @f$\gamma@f$.
 *
 * ## Template parameter requirement
 *
 * - SCALAR must be a type like `double`
 * - DIFF_COEFF must provide an evaluation operator
 * `operator (const Entity &,ref_coord_t)` that returns either a scalar
 * or a matrix type that is compatible with Eigen's matrices. Usually it will
 * be an Eigen::Matrix either of variable of fixed size.
 *
 */
template <typename SCALAR, typename DIFF_COEFF, typename REACTION_COEFF>
class ReactionDiffusionElementMatrixProvider {
  static_assert(isMeshFunction<DIFF_COEFF>);
  static_assert(isMeshFunction<REACTION_COEFF>);

 public:
  /**
   * @brief type of returned element matrix
   */
  using elem_mat_t = Eigen::Matrix<SCALAR, Eigen::Dynamic, Eigen::Dynamic>;
  /** @brief Return type for @ref Eval() method */
  using ElemMat = const elem_mat_t;

  /** @brief standard constructors */
  /** @{ */
  ReactionDiffusionElementMatrixProvider(
      const ReactionDiffusionElementMatrixProvider &) = delete;
  ReactionDiffusionElementMatrixProvider(
      ReactionDiffusionElementMatrixProvider &&) noexcept = default;
  ReactionDiffusionElementMatrixProvider &operator=(
      const ReactionDiffusionElementMatrixProvider &) = delete;
  ReactionDiffusionElementMatrixProvider &operator=(
      ReactionDiffusionElementMatrixProvider &&) = delete;
  /** @} */

  /**
   * @brief Constructor: cell-independent precomputations
   *
   * @param fe_trie_p finite element to be used on triangles
   * @param fe_quad_p finite element for quadrilaterals
   *
   * @see LocCompLagrFEPreprocessor::LocCompLagrFEPreprocessor()
   */
  ReactionDiffusionElementMatrixProvider(
      std::shared_ptr<ScalarUniformFESpace<SCALAR>> fe_space, DIFF_COEFF alpha,
      REACTION_COEFF gamma);
  /**
   * @brief All cells are considered active in the default implementation
   *
   * This method is meant to be overloaded if assembly should be restricted to a
   * subset of cells.
   */
  virtual bool isActive(const lf::mesh::Entity & /*cell*/) { return true; }
  /*
   * @brief main routine for the computation of element matrices
   *
   * @param cell reference to the (triangular or quadrilateral) cell for
   *        which the element matrix should be computed.
   * @return a small dense, containing the element matrix.
   *
   * Actual computation of the element matrix based on numerical quadrature and
   * mapping techniques. The order of the quadrature rule is tied to the
   * polynomial degree of the underlying Lagrangian finite element spaces: for
   * polynomial degree p a quadrature rule is chosen that is exact for
   * polynomials o degree 2p.
   *
   * Throws an assertion in case the finite element specification is missing for
   * the type of the cell.
   */
  ElemMat Eval(const lf::mesh::Entity &cell);

  /** Virtual destructor */
  virtual ~ReactionDiffusionElementMatrixProvider() = default;

 private:
  /** @defgroup coefficient functors
   * @brief functors providing coefficient functions
   * @{ */
  /** Diffusion coefficient */
  DIFF_COEFF alpha_;
  /** Reaction coefficient */
  REACTION_COEFF gamma_;
  /** @} */

  // fe_precomp_[i] contains precomputed reference finite element for ref_el i.
  std::array<PrecomputedScalarReferenceFiniteElement<SCALAR>, 5> fe_precomp_;

 public:
  /** @brief output control variable */
  static unsigned int ctrl_;
  static const unsigned int kout_cell = 8;
  static const unsigned int kout_locmat = 16;
};

template <typename SCALAR, typename DIFF_COEFF, typename REACTION_COEFF>
unsigned int ReactionDiffusionElementMatrixProvider<SCALAR, DIFF_COEFF,
                                                    REACTION_COEFF>::ctrl_ = 0;

template <typename SCALAR, typename DIFF_COEFF, typename REACTION_COEFF>
ReactionDiffusionElementMatrixProvider<SCALAR, DIFF_COEFF, REACTION_COEFF>::
    ReactionDiffusionElementMatrixProvider(
        std::shared_ptr<ScalarUniformFESpace<SCALAR>> fe_space,
        DIFF_COEFF alpha, REACTION_COEFF gamma)
    : alpha_(std::move(alpha)), gamma_(std::move(gamma)), fe_precomp_() {
  for (auto ref_el : {base::RefEl::kTria(), base::RefEl::kQuad()}) {
    auto fe = fe_space->ShapeFunctionLayout(ref_el);
    fe_precomp_[ref_el.Id()] = PrecomputedScalarReferenceFiniteElement(
        fe, quad::make_QuadRule(ref_el, 2 * fe->Degree()));
  }
}

// TODO(craffael) remove const once
// https://developercommunity.visualstudio.com/content/problem/180948/vs2017-155-c-cv-qualifiers-lost-on-type-alias-used.html
// is resolved
template <typename SCALAR, typename DIFF_COEFF, typename REACTION_COEFF>
typename lf::uscalfe::ReactionDiffusionElementMatrixProvider<
    SCALAR, DIFF_COEFF, REACTION_COEFF>::ElemMat const
ReactionDiffusionElementMatrixProvider<
    SCALAR, DIFF_COEFF, REACTION_COEFF>::Eval(const lf::mesh::Entity &cell) {
  // Topological type of the cell
  const lf::base::RefEl ref_el{cell.RefEl()};
  PrecomputedScalarReferenceFiniteElement<SCALAR> &pfe =
      fe_precomp_[ref_el.Id()];

  // Query the shape of the cell
  const lf::geometry::Geometry *geo_ptr = cell.Geometry();
  LF_ASSERT_MSG(geo_ptr != nullptr, "Invalid geometry!");
  LF_ASSERT_MSG((geo_ptr->DimLocal() == 2),
                "Only 2D implementation available!");
  SWITCHEDSTATEMENT(ctrl_, kout_cell,
                    std::cout << ref_el << ", shape = \n"
                              << geo_ptr->Global(ref_el.NodeCoords())
                              << std::endl);
  // Physical dimension of the cell
  const dim_t world_dim = geo_ptr->DimGlobal();

  const Eigen::VectorXd determinants(
      geo_ptr->IntegrationElement(pfe.Qr().Points()));
  LF_ASSERT_MSG(
      determinants.size() == pfe.Qr().NumPoints(),
      "Mismatch " << determinants.size() << " <-> " << pfe.Qr().NumPoints());
  // Fetch the transformation matrices for the gradients
  const Eigen::MatrixXd JinvT(
      geo_ptr->JacobianInverseGramian(pfe.Qr().Points()));
  LF_ASSERT_MSG(
      JinvT.cols() == 2 * pfe.Qr().NumPoints(),
      "Mismatch " << JinvT.cols() << " <-> " << 2 * pfe.Qr().NumPoints());
  LF_ASSERT_MSG(JinvT.rows() == world_dim,
                "Mismatch " << JinvT.rows() << " <-> " << world_dim);

  // compute values of alpha, gamma at quadrature points:
  auto alphaval = alpha_(cell, pfe.Qr().Points());
  auto gammaval = gamma_(cell, pfe.Qr().Points());

  // Element matrix
  elem_mat_t mat(pfe.NumRefShapeFunctions(), pfe.NumRefShapeFunctions());
  mat.setZero();

  // Loop over quadrature points
  for (int k = 0; k < pfe.Qr().NumPoints(); ++k) {
    const double w = pfe.Qr().Weights()[k] * determinants[k];
    // Transformed gradients
    const auto trf_grad(JinvT.block(0, 2 * k, world_dim, 2) *
                        pfe.PrecompGradientsReferenceShapeFunctions()
                            .block(0, 2 * k, mat.rows(), 2)
                            .transpose());
    // Transformed gradients multiplied with coefficient
    const auto alpha_trf_grad(alphaval[k] * trf_grad);
    mat += w * (alpha_trf_grad.transpose() * trf_grad +
                (gammaval[k] * pfe.PrecompReferenceShapeFunctions().col(k)) *
                    (pfe.PrecompReferenceShapeFunctions().col(k).transpose()));
  }
  return mat;
}

/** @brief Quadrature-based computation of local mass matrix for an edge
 *
 * @tparam SCALAR underlying scalar type, usually double or complex<double>
 * @tparam COEFF \ref mesh_function "MeshFunction" that defines the
 * scalar valued coefficient \f$ \gamma \f$
 * @tparam EDGESELECTOR predicate defining which edges are included
 *
 * This helper class corresponds to the the element matrix
 * for the bilinear form
 * @f[
 *     (u,v) \mapsto \int\limits_e \gamma(x)u(x)v(x)\,\mathrm{d}S(x)\;,
 * @f]
 * where @f$e@f$ is an edge of the mesh, and @f$\gamma@f$ a scalar-valued
 * coefficient function.
 *
 * @sa MassEdgeMatrixProvider
 */
template <typename SCALAR, typename COEFF, typename EDGESELECTOR>
class MassEdgeMatrixProvider {
 public:
  using scalar_t = decltype(SCALAR(0) * MeshFunctionReturnType<COEFF>(0));
  using elem_mat_t = Eigen::Matrix<scalar_t, Eigen::Dynamic, Eigen::Dynamic>;
  using ElemMat = const elem_mat_t;

  /** @defgroup
      @brief standard constructors
     * @{ */
  MassEdgeMatrixProvider(const MassEdgeMatrixProvider &) = delete;
  MassEdgeMatrixProvider(MassEdgeMatrixProvider &&) noexcept = default;
  MassEdgeMatrixProvider &operator=(const MassEdgeMatrixProvider &) = delete;
  MassEdgeMatrixProvider &operator=(MassEdgeMatrixProvider &&) = delete;
  /** @} */
  /**
   * @brief Constructor performing cell-independent initializations
   *
   * @param fe_space Describes the shapefunctions
   * @param gamma coefficient function through functor object
   * @param edge_selector predicate object selecting active to be covered in
   the
   * assembly
   */
  MassEdgeMatrixProvider(std::shared_ptr<ScalarUniformFESpace<SCALAR>> fe_space,
                         COEFF gamma,
                         EDGESELECTOR edge_selector = base::PredicateTrue{})
      : gamma_(std::move(gamma)),
        edge_sel_(std::move(edge_selector)),
        fe_precomp_() {
    auto fe = fe_space->ShapeFunctionLayout(base::RefEl::kSegment());
    fe_precomp_ = PrecomputedScalarReferenceFiniteElement(
        fe, quad::make_QuadRule(base::RefEl::kSegment(), 2 * fe->Degree()));
  }

  /**
   * @brief If true, then an edge is taken into account during assembly
   *
   * The information about "active" edges is supplied through the
   * `edge_selector` argument of the constructor.
   */
  bool isActive(const lf::mesh::Entity &edge) {
    LF_ASSERT_MSG(edge.RefEl() == lf::base::RefEl::kSegment(),
                  "Wrong type for an edge");
    return edge_sel_(edge);
  }

  /**
   * @brief actual computation of edge mass matrix
   *
   * @param edge reference to the edge for
   *        which the mass matrix is needed
   * @return a small dense matrix, containing the element matrix.
   *
   * Actual computation of the local edge mass based on numerical quadrature
   and
   * mapping techniques. The order of the quadrature rule is tied to the
   * polynomial degree of the underlying Lagrangian finite element spaces:
   for
   * polynomial degree p a quadrature rule is chosen that is exact for
   * polynomials o degree 2p.
   */
  ElemMat Eval(const lf::mesh::Entity &edge);

  virtual ~MassEdgeMatrixProvider() = default;

 private:
  COEFF gamma_;               // functor for coefficient
  EDGESELECTOR edge_sel_;     // Defines the active edges
  static unsigned int ctrl_;  // output control variable
  PrecomputedScalarReferenceFiniteElement<SCALAR> fe_precomp_;
};

// deduction guide:
template <class PTR, class COEFF, class EDGESELECTOR = base::PredicateTrue>
MassEdgeMatrixProvider(PTR, COEFF coeff,
                       EDGESELECTOR edge_predicate = base::PredicateTrue{})
    ->MassEdgeMatrixProvider<typename PTR::element_type::Scalar, COEFF,
                             EDGESELECTOR>;

template <class SCALAR, class COEFF, class EDGESELECTOR>
unsigned int MassEdgeMatrixProvider<SCALAR, COEFF, EDGESELECTOR>::ctrl_ = 0;

// Eval() method
// TODO(craffael) remove const once
// https://
// developercommunity.visualstudio.com/content/problem/180948/vs2017-155-c-cv-qualifiers-lost-on-type-alias-used.html
// is resolved
template <class SCALAR, class COEFF, class EDGESELECTOR>
typename MassEdgeMatrixProvider<SCALAR, COEFF, EDGESELECTOR>::ElemMat const
MassEdgeMatrixProvider<SCALAR, COEFF, EDGESELECTOR>::Eval(
    const lf::mesh::Entity &edge) {
  // Topological type of the cell
  const lf::base::RefEl ref_el{edge.RefEl()};
  LF_ASSERT_MSG(ref_el == lf::base::RefEl::kSegment(),
                "Edge must be of segment type");
  // Query the shape of the edge
  const lf::geometry::Geometry *geo_ptr = edge.Geometry();
  LF_ASSERT_MSG(geo_ptr != nullptr, "Invalid geometry!");

  // Obtain the metric factors for the quadrature points
  const Eigen::VectorXd determinants(
      geo_ptr->IntegrationElement(fe_precomp_.Qr().Points()));
  LF_ASSERT_MSG(determinants.size() == fe_precomp_.Qr().NumPoints(),
                "Mismatch " << determinants.size() << " <-> "
                            << fe_precomp_.Qr().NumPoints());

  // Element matrix
  elem_mat_t mat(fe_precomp_.NumRefShapeFunctions(),
                 fe_precomp_.NumRefShapeFunctions());
  mat.setZero();

  auto gammaval = gamma_(edge, fe_precomp_.Qr().Points());

  // Loop over quadrature points
  for (int k = 0; k < determinants.size(); ++k) {
    // Build local matrix by summing rank-1 contributions
    // from quadrature points.
    const auto w =
        (fe_precomp_.Qr().Weights()[k] * determinants[k]) * gammaval[k];
    mat += ((fe_precomp_.PrecompReferenceShapeFunctions().col(k)) *
            (fe_precomp_.PrecompReferenceShapeFunctions().col(k).transpose())) *
           w;
  }
  return mat;
}

/**
 * @brief Local computation of general element (load) vector for scalar
 finite
 * elements; volume contributions only
 *
 * @tparam SCALAR underlying scalar type, usually double or complex<double>
 * @tparam FUNCTOR \ref mesh_function "MeshFunction" which defines the source
 * function \f$ f \f$
 *
 * The underlying local linear form is
 * @f[
      v \mapsto \int_K f(\mathbf{x})\,v(\mathbf{x}\,\mathrm{d}\mathbf{x}\;,
 * @f]
 * where \f$f\f$ is supposed to be a locally continuous source function.
 *
 * Computation is based on a quadrature rules supplied by the LehrFEM++
 * lf::quad::QuadRule module.
 *
 * This class complies with the requirements for the template parameter
 * `ELEM_VEC_COMP` of the function AssembleVectorLocally().
 */
template <typename SCALAR, typename FUNCTOR>
class ScalarLoadElementVectorProvider {
  static_assert(isMeshFunction<FUNCTOR>);

 public:
  using elem_vec_t = Eigen::Matrix<SCALAR, Eigen::Dynamic, 1>;
  using ElemVec = const elem_vec_t;

  /** @defgroup stdc
   * @brief standard constructors
   *@{*/
  ScalarLoadElementVectorProvider(const ScalarLoadElementVectorProvider &) =
      delete;
  ScalarLoadElementVectorProvider(ScalarLoadElementVectorProvider &&) noexcept =
      default;
  ScalarLoadElementVectorProvider &operator=(
      const ScalarLoadElementVectorProvider &) = delete;
  ScalarLoadElementVectorProvider &operator=(
      ScalarLoadElementVectorProvider &&) = delete;
  /**@}*/

  /** @brief Constructor, performs precomputations
   *
   * @param fe_tria_p pointer to local shape functions to be used on
   triangles
   * @param fe_quad_p pointer local shape functions for quadrilaterals
   * @param f functor object for source function
   *
   * Either pointer may be NULL, which is acceptable, if local computatioins
   for
   * that element type are not requested.
   */
  ScalarLoadElementVectorProvider(
      std::shared_ptr<ScalarUniformFESpace<SCALAR>> fe_space, FUNCTOR f);
  /** @brief Default implement: all cells are active */
  virtual bool isActive(const lf::mesh::Entity & /*cell*/) { return true; }
  /*
   * @brief Main method for computing the element vector
   *
   * @param cell current cell for which the element vector is desired
   * @return local load vector as column vector
   *
   */
  ElemVec Eval(const lf::mesh::Entity &cell);

  virtual ~ScalarLoadElementVectorProvider() = default;

 private:
  /** @brief An object providing the source function */
  FUNCTOR f_;

  std::array<PrecomputedScalarReferenceFiniteElement<SCALAR>, 5> fe_precomp_;

 public:
  /*
   * @brief static variable for controlling (debugging) output
   */
  static unsigned int ctrl_;
  static const unsigned int kout_cell = 8;
  static const unsigned int kout_locvec = 16;
  static const unsigned int kout_dets = 32;
  static const unsigned int kout_loop = 64;
  static const unsigned int kout_qpts = 128;
};

template <typename SCALAR, typename FUNCTOR>
unsigned int ScalarLoadElementVectorProvider<SCALAR, FUNCTOR>::ctrl_ = 0;

// Constructors
template <typename SCALAR, typename FUNCTOR>
ScalarLoadElementVectorProvider<SCALAR, FUNCTOR>::
    ScalarLoadElementVectorProvider(
        std::shared_ptr<ScalarUniformFESpace<SCALAR>> fe_space, FUNCTOR f)
    : f_(std::move(f)) {
  for (auto ref_el : {base::RefEl::kTria(), base::RefEl::kQuad()}) {
    auto fe = fe_space->ShapeFunctionLayout(ref_el);
    fe_precomp_[ref_el.Id()] = PrecomputedScalarReferenceFiniteElement<SCALAR>(
        fe, quad::make_QuadRule(ref_el, 2 * fe->Degree()));
  }
}

// TODO(craffael) remove const once
// http://developercommunity.visualstudio.com/content/problem/180948/vs2017-155-c-cv-qualifiers-lost-on-type-alias-used.html
// is resolved
template <typename SCALAR, typename FUNCTOR>
typename ScalarLoadElementVectorProvider<SCALAR, FUNCTOR>::ElemVec const
ScalarLoadElementVectorProvider<SCALAR, FUNCTOR>::Eval(
    const lf::mesh::Entity &cell) {
  // Type for source function
  using source_fn_t = MeshFunctionReturnType<FUNCTOR>;
  // Topological type of the cell
  const lf::base::RefEl ref_el{cell.RefEl()};
  auto &pfe = fe_precomp_[ref_el.Id()];
  // Query the shape of the cell
  const lf::geometry::Geometry *geo_ptr = cell.Geometry();
  LF_ASSERT_MSG(geo_ptr != nullptr, "Invalid geometry!");
  LF_ASSERT_MSG((geo_ptr->DimLocal() == 2),
                "Only 2D implementation available!");
  SWITCHEDSTATEMENT(ctrl_, kout_cell,
                    std::cout << ref_el << ", shape = \n"
                              << geo_ptr->Global(ref_el.NodeCoords())
                              << std::endl);

  // Obtain the metric factors for the quadrature points
  const Eigen::VectorXd determinants(
      geo_ptr->IntegrationElement(pfe.Qr().Points()));
  LF_ASSERT_MSG(
      determinants.size() == pfe.Qr().NumPoints(),
      "Mismatch " << determinants.size() << " <-> " << pfe.Qr().NumPoints());
  SWITCHEDSTATEMENT(ctrl_, kout_dets,
                    std::cout << "LOCVEC(" << ref_el << "): Metric factors :\n "
                              << determinants.transpose() << std::endl);
  // Element vector
  elem_vec_t vec(pfe.NumRefShapeFunctions());
  vec.setZero();

  auto fval = f_(cell, pfe.Qr().Points());

  // Loop over quadrature points
  for (int k = 0; k < determinants.size(); ++k) {
    SWITCHEDSTATEMENT(
        ctrl_, kout_loop,
        std::cout << "LOCVEC: [" << pfe.Qr().Points().transpose() << "] -> ["
                  << "weight = " << pfe.Qr().Weights()[k] << std::endl);
    // Contribution of current quadrature point
    vec += (pfe.Qr().Weights()[k] * determinants[k] * fval[k]) *
           pfe.PrecompReferenceShapeFunctions().col(k);
  }
  SWITCHEDSTATEMENT(ctrl_, kout_locvec,
                    std::cout << "LOCVEC = \n"
                              << vec.transpose() << std::endl);
  return vec;
}

/**
 * @brief Local edge contributions to element vector
 *
 * @tparam SCALAR underlying scalar type, usually double or complex<double>
 * @tparam FUNCTOR `SCALAR` valued \ref mesh_function "MeshFunction" which
 * defines the function \f$ g \f$
 *
 * The underlying local linear form for an edge @f$e@f$ is
 * @f[
    v \mapsto \int_e g(\mathbf{x})\,v(\mathbf{x})\,\mathrm{d}S\mathbf{x}\;,
 * @f]
 * where \f$g\f$ is supposed to be a locally continuous source function.
 *
 * Computation is based on a quadrature rules supplied by the LehrFEM++
 * lf::quad::QuadRule module.
 *
 * This class complies with the requirements for the template parameter
 * `ELEM_VEC_COMP` of the function AssembleVectorLocally().
 */
template <class SCALAR, class FUNCTOR, class EDGESELECTOR = base::PredicateTrue>
class ScalarLoadEdgeVectorProvider {
 public:
  using elem_vec_t = Eigen::Matrix<SCALAR, Eigen::Dynamic, 1>;
  using ElemVec = const elem_vec_t;

  /** @defgroup stdc
   * @brief standard constructors
   *@{*/
  ScalarLoadEdgeVectorProvider(const ScalarLoadEdgeVectorProvider &) = delete;
  ScalarLoadEdgeVectorProvider(ScalarLoadEdgeVectorProvider &&) noexcept =
      default;
  ScalarLoadEdgeVectorProvider &operator=(
      const ScalarLoadEdgeVectorProvider &) = delete;
  ScalarLoadEdgeVectorProvider &operator=(ScalarLoadEdgeVectorProvider &&) =
      delete;
  /**@}*/

  /** @brief Constructor, performs precomputations
   *
   * @param fe_edge_p FE specification on edge
   * @param g functor object providing edge data
   */
  ScalarLoadEdgeVectorProvider(
      std::shared_ptr<const ScalarUniformFESpace<SCALAR>> fe_space, FUNCTOR g,
      EDGESELECTOR edge_sel = base::PredicateTrue{})
      : g_(std::move(g)), edge_sel_(std::move(edge_sel)), pfe_() {
    auto fe = fe_space->ShapeFunctionLayout(base::RefEl::kSegment());
    pfe_ = PrecomputedScalarReferenceFiniteElement(
        fe, quad::make_QuadRule(base::RefEl::kSegment(), 2 * fe->Degree()));
  }
  /** @brief Default implement: all edges are active */
  virtual bool isActive(const lf::mesh::Entity &cell) {
    return edge_sel_(cell);
  }
  /*
   * @brief Main method for computing the element vector
   *
   * @param cell current cell for which the element vector is desired
   * @return local load vector as column vector
   *
   */
  ElemVec Eval(const lf::mesh::Entity &edge);

  virtual ~ScalarLoadEdgeVectorProvider() = default;

 private:
  FUNCTOR g_;              // source function
  EDGESELECTOR edge_sel_;  // selects edges
  PrecomputedScalarReferenceFiniteElement<SCALAR> pfe_;

 public:
  /*
   * @brief static variable for controlling (debugging) output
   */
  static unsigned int ctrl_;
  static const unsigned int kout_cell = 8;
  static const unsigned int kout_locvec = 16;
  static const unsigned int kout_dets = 32;
  static const unsigned int kout_loop = 64;
  static const unsigned int kout_qpts = 128;
};

// deduction guide
template <class PTR, class FUNCTOR, class EDGESELECTOR = base::PredicateTrue>
ScalarLoadEdgeVectorProvider(PTR, FUNCTOR, EDGESELECTOR = base::PredicateTrue{})
    ->ScalarLoadEdgeVectorProvider<typename PTR::element_type::Scalar, FUNCTOR,
                                   EDGESELECTOR>;

template <class SCALAR, class FUNCTOR, class EDGESELECTOR>
unsigned int
    ScalarLoadEdgeVectorProvider<SCALAR, FUNCTOR, EDGESELECTOR>::ctrl_ = 0;

// Eval() method
// TODO(craffael) remove const once
// https://developercommunity.visualstudio.com/content/problem/180948/vs2017-155-c-cv-qualifiers-lost-on-type-alias-used.html
// is resolved
template <class SCALAR, class FUNCTOR, class EDGESELECTOR>
typename ScalarLoadEdgeVectorProvider<SCALAR, FUNCTOR,
                                      EDGESELECTOR>::ElemVec const
ScalarLoadEdgeVectorProvider<SCALAR, FUNCTOR, EDGESELECTOR>::Eval(
    const lf::mesh::Entity &edge) {
  // Topological type of the cell
  const lf::base::RefEl ref_el{edge.RefEl()};
  LF_ASSERT_MSG(ref_el == lf::base::RefEl::kSegment(),
                "Edge must be of segment type");
  // Query the shape of the edge
  const lf::geometry::Geometry *geo_ptr = edge.Geometry();
  LF_ASSERT_MSG(geo_ptr != nullptr, "Invalid geometry!");

  // Quadrature points on physical edge
  const Eigen::MatrixXd mapped_qpts(geo_ptr->Global(pfe_.Qr().Points()));
  LF_ASSERT_MSG(
      mapped_qpts.cols() == pfe_.Qr().NumPoints(),
      "Mismatch " << mapped_qpts.cols() << " <-> " << pfe_.Qr().NumPoints());

  // Obtain the metric factors for the quadrature points
  const Eigen::VectorXd determinants(
      geo_ptr->IntegrationElement(pfe_.Qr().Points()));
  LF_ASSERT_MSG(
      determinants.size() == pfe_.Qr().NumPoints(),
      "Mismatch " << determinants.size() << " <-> " << pfe_.Qr().NumPoints());

  // Element vector
  elem_vec_t vec(pfe_.NumRefShapeFunctions());
  vec.setZero();

  auto g_vals = g_(edge, pfe_.Qr().Points());

  // Loop over quadrature points
  for (int k = 0; k < pfe_.Qr().NumPoints(); ++k) {
    // Add contribution of quadrature point to local vector
    const auto w = (pfe_.Qr().Weights()[k] * determinants[k]) * g_vals[k];
    vec += pfe_.PrecompReferenceShapeFunctions().col(k) * w;
  }
  return vec;
}

}  // namespace lf::uscalfe

#endif
