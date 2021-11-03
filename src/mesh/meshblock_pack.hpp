#ifndef MESH_MESHBLOCK_PACK_HPP_
#define MESH_MESHBLOCK_PACK_HPP_
//========================================================================================
// AthenaXXX astrophysical plasma code
// Copyright(C) 2020 James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file meshblock_pack.hpp
//  \brief defines MeshBlockPack class, a container for MeshBlocks

#include "parameter_input.hpp"
#include "coordinates/coordinates.hpp"
#include "driver/driver.hpp"
#include "tasklist/task_list.hpp"

// Forward declarations
class MeshBlock;
namespace hydro {class Hydro;}
namespace mhd {class MHD;}
class IonNeutral;
class TurbulenceDriver;

//----------------------------------------------------------------------------------------
//! \class MeshBlock
//  \brief data/functions associated with a single block

class MeshBlockPack
{
  // mesh classes (Mesh, MeshBlock, MeshBlockPack, MeshBlockTree) like to play together
  friend class Mesh;
  friend class MeshBlock;
  friend class MeshBlockTree;

public:
  MeshBlockPack(Mesh *pm, int igids, int igide);
  ~MeshBlockPack();

  // data
  Mesh *pmesh;            // ptr to Mesh containing this MeshBlockPack
  int gids, gide;         // start/end of global IDs in this MeshBlockPack
  int nmb_thispack;       // number of MBs in this pack

  // following Grid/Physics objects are all pointers so they can be allocated after
  // MeshBlockPack is constructed with pointer to my_pack. 

  MeshBlock* pmb;         // MeshBlocks in this MeshBlockPack
  Coordinates* pcoord;

  // physics modules (controlled by AddPhysicsModules function in mesh_physics.cpp)
  hydro::Hydro *phydro=nullptr;
  mhd::MHD *pmhd=nullptr;
  IonNeutral *pionn=nullptr;
  TurbulenceDriver *pturb=nullptr;

  // task lists for all MeshBlocks in this MeshBlockPack
  TaskList operator_split_tl;            // operator-split physics
  TaskList start_tl, run_tl, end_tl;     // each stage of RK integrators

  // functions
  void AddPhysicsModules(ParameterInput *pin, Driver *pdrive);
  void AddMeshBlocksAndCoordinates(ParameterInput *pin, RegionIndcs indcs);
  int NumberOfMeshBlockCells() const {
    return (pcoord->mbdata.indcs.nx1)*(pcoord->mbdata.indcs.nx2)*
           (pcoord->mbdata.indcs.nx3);
  }

private:
  // data

  // functions
  void SetNeighbors(std::unique_ptr<MeshBlockTree> &ptree, int *ranklist);

};

#endif // MESH_MESHBLOCK_PACK_HPP_
