/**
 * @file
 * @brief We build a tensor product mesh on a torus and apply uniform refinement
 * @author Anian Ruoss
 * @date   2018-10-20 16:27:17
 * @copyright MIT License
 */

#include <boost/program_options.hpp>
#include <iostream>

#include <lf/refinement/mesh_hierarchy.h>
#include "lf/io/io.h"

using size_type = lf::base::size_type;

int main(int argc, char **argv) {
  // define allowed command line arguments:
  namespace po = boost::program_options;
  po::options_description desc("Allowed options");
  desc.add_options()("help", "Produce this help message")(
      "num_steps", po::value<size_t>()->default_value(4),
      "Number of uniform refinement steps")(
      "num_x_cells", po::value<size_type>()->default_value(4),
      "Number of cells in x direction")(
      "num_y_cells", po::value<size_type>()->default_value(4),
      "Number of cells in y direction")(
      "top_right_corner",
      po::value<std::vector<double>>()->multitoken()->default_value(
          std::vector<double>{1., 4.}, "1., 4."),
      "Coordinates of top right corner of rectangle with bottom left at (0,0)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help") != 0u) {
    std::cout << desc << std::endl;
    return 1;
  }

  const size_t num_steps = vm["num_steps"].as<size_t>();
  const size_type num_x_cells = vm["num_x_cells"].as<size_type>();
  const size_type num_y_cells = vm["num_y_cells"].as<size_type>();
  std::vector<double> top_right_corner_coords =
      vm["top_right_corner"].as<std::vector<double>>();

  std::shared_ptr<lf::mesh::hybrid2d::MeshFactory> mesh_factory_ptr =
      std::make_shared<lf::mesh::hybrid2d::MeshFactory>(3);

  // build tensor product mesh by specifying rectangle from which torus will
  // be generated by identifying opposite edges
  lf::mesh::hybrid2d::TorusMeshBuilder builder(mesh_factory_ptr);
  builder.setBottomLeftCorner(Eigen::Vector2d{0., 0.});
  builder.setTopRightCorner(Eigen::Vector2d{top_right_corner_coords.data()});
  builder.setNoXCells(num_x_cells);
  builder.setNoYCells(num_y_cells);
  std::shared_ptr<lf::mesh::Mesh> mesh_ptr = builder.Build();

  // output mesh information
  const lf::mesh::Mesh &mesh = *mesh_ptr;
  lf::mesh::utils::PrintInfo(mesh, std::cout);
  std::cout << std::endl;

  // build mesh hierarchy
  lf::refinement::MeshHierarchy multi_mesh(mesh_ptr, mesh_factory_ptr);

  for (int step = 0; step < num_steps; ++step) {
    // obtain pointer to mesh on finest level
    const size_type n_levels = multi_mesh.NumLevels();
    std::shared_ptr<const lf::mesh::Mesh> mesh_fine =
        multi_mesh.getMesh(n_levels - 1);

    // print number of entities of various co-dimensions
    std::cout << "Mesh on level " << n_levels - 1 << ": "
              << mesh_fine->NumEntities(2) << " nodes, "
              << mesh_fine->NumEntities(1) << " edges, "
              << mesh_fine->NumEntities(0) << " cells," << std::endl;

    lf::io::writeMatplotlib(*mesh_fine, std::string("torus_refinement") +
                                            std::to_string(step) + ".csv");

    lf::io::VtkWriter vtk_writer(mesh_fine, std::string("torus_refinement") +
                                                std::to_string(step) + ".vtk");

    multi_mesh.RefineRegular();
  }

  return 0;
}
