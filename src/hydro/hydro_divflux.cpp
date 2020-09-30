//========================================================================================
// AthenaXXX astrophysical plasma code
// Copyright(C) 2020 James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file calculate_divflux.cpp
//  \brief Calculate divergence of the fluxes for hydro only, no mesh refinement

#include <iostream>

#include "athena.hpp"
#include "mesh/mesh.hpp"
#include "hydro.hpp"
#include "hydro/eos/eos.hpp"
// include inlined reconstruction methods (yuck...)
#include "reconstruct/dc.cpp"
#include "reconstruct/plm.cpp"
#include "reconstruct/ppm.cpp"
// include inlined Riemann solvers (double yuck...)
#include "hydro/rsolver/advect.cpp"
#include "hydro/rsolver/llf.cpp"
#include "hydro/rsolver/hllc.cpp"
#include "hydro/rsolver/roe.cpp"

namespace hydro {
//----------------------------------------------------------------------------------------
//! \fn  void Hydro::CalculateDivFlux
//  \brief Calculate divergence of the fluxes for hydro only, no mesh refinement

TaskStatus Hydro::HydroDivFlux(Driver *pdrive, int stage)
{
  MeshBlock *pmb = pmesh_->FindMeshBlock(my_mbgid_);
  int is = pmb->mb_cells.is; int ie = pmb->mb_cells.ie;
  int js = pmb->mb_cells.js; int je = pmb->mb_cells.je;
  int ks = pmb->mb_cells.ks; int ke = pmb->mb_cells.ke;
  int ncells1 = pmb->mb_cells.nx1 + 2*(pmb->mb_cells.ng);

  int nhydro_ = nhydro;
  auto recon_method = recon_method_;
  auto rsolver_method = rsolver_method_;
  auto &eos = pmb->phydro->peos->eos_data;
  auto &w0_ = w0;
  auto &divf_ = divf;

  //--------------------------------------------------------------------------------------
  // i-direction

  size_t scr_size = AthenaScratch2D<Real>::shmem_size(nhydro, ncells1) * 3;
  int scr_level = 0;
  Real &dx1 = pmb->mb_cells.dx1;

  par_for_outer("divflux_x1", pmb->exe_space, scr_size, scr_level, ks, ke, js, je,
    KOKKOS_LAMBDA(TeamMember_t member, const int k, const int j)
    {
      AthenaScratch2D<Real> wl(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> wr(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> uflux(member.team_scratch(scr_level), nhydro_, ncells1);

      // Reconstruction qR[i] and qL[i+1]
      switch (recon_method)
      {
        case ReconstructionMethod::dc:
          DonorCellX1(member, k, j, is-1, ie+1, w0_, wl, wr);
          break;
        case ReconstructionMethod::plm:
          PiecewiseLinearX1(member, k, j, is-1, ie+1, w0_, wl, wr);
          break;
        case ReconstructionMethod::ppm:
          PiecewiseParabolicX1(member, k, j, is-1, ie+1, w0_, wl, wr);
          break;
        default:
          break;
      }
      // Sync all threads in the team so that scratch memory is consistent
      member.team_barrier();

      // compute fluxes over [is,ie+1]
      switch (rsolver_method)
      {
        case RiemannSolver::advect:
          Advect(member, eos, is, ie+1, IVX, wl, wr, uflux);
          break;
        case RiemannSolver::llf:
          LLF(member, eos, is, ie+1, IVX, wl, wr, uflux);
          break;
        case RiemannSolver::hllc:
          HLLC(member, eos, is, ie+1, IVX, wl, wr, uflux);
          break;
        case RiemannSolver::roe:
          Roe(member, eos, is, ie+1, IVX, wl, wr, uflux);
          break;
        default:
          break;
      }
      member.team_barrier();

      // compute dF/dx1
      for (int n=0; n<nhydro_; ++n) {
        par_for_inner(member, is, ie, [&](const int i)
        {
          divf_(n,k,j,i) = (uflux(n,i+1) - uflux(n,i))/dx1;
        });
      }
      member.team_barrier();
    }
  );
  if (!(pmesh_->nx2gt1)) return TaskStatus::complete;

  //--------------------------------------------------------------------------------------
  // j-direction

  scr_size = AthenaScratch2D<Real>::shmem_size(nhydro, ncells1) * 4;
  Real &dx2 = pmb->mb_cells.dx2;

  par_for_outer("divflux_x2", pmb->exe_space, scr_size, scr_level, ks, ke,
    KOKKOS_LAMBDA(TeamMember_t member, const int k)
    {
      AthenaScratch2D<Real> wl_flx(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> wr(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> wl_jp1(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> uflux_jm1(member.team_scratch(scr_level), nhydro_, ncells1);

      for (int j=js-1; j<=je+1; ++j) {
        // copy Wl from last iteration of j (unless this is the first time through)
        if (j>(js-1)) {
          wl_flx = wl_jp1;
        }

        // Reconstruction qR[j] and qL[j+1]
        switch (recon_method)
        {
          case ReconstructionMethod::dc:
            DonorCellX2(member, k, j, is, ie, w0_, wl_jp1, wr);
            break;
          case ReconstructionMethod::plm:
            PiecewiseLinearX2(member, k, j, is, ie, w0_, wl_jp1, wr);
            break;
          case ReconstructionMethod::ppm:
            PiecewiseParabolicX2(member, k, j, is, ie, w0_, wl_jp1, wr);
            break;
          default:
            break;
        }
        member.team_barrier();

        // compute fluxes over [js,je+1]
        if (j>(js-1)) {
          switch (rsolver_method)
          {
            case RiemannSolver::advect:
              Advect(member, eos, is, ie, IVY, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::llf:
              LLF(member, eos, is, ie, IVY, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::hllc:
              HLLC(member, eos, is, ie, IVY, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::roe:
              Roe(member, eos, is, ie, IVY, wl_flx, wr, wl_flx);
              break;
            default:
              break;
          }
        }
        member.team_barrier();

        // Add dF/dx2
        // Fluxes must be summed together to symmetrize round-off error in each dir
        if (j>js) {
          for (int n=0; n<nhydro_; ++n) {
            par_for_inner(member, is, ie, [&](const int i)
            {
              divf_(n,k,j-1,i) += (wl_flx(n,i) - uflux_jm1(n,i))/dx2;
            });
          }
        }
        member.team_barrier();
  
        // copy flux for use in next iteration
        if (j>(js-1) && j<(je+1)) {
          uflux_jm1 = wl_flx;
        }
      } // end of loop over j
    }
  );
  if (!(pmesh_->nx3gt1)) return TaskStatus::complete;

  //--------------------------------------------------------------------------------------
  // k-direction. Note order of k,j loops switched

  scr_size = AthenaScratch2D<Real>::shmem_size(nhydro, ncells1) * 4;
  Real &dx3 = pmb->mb_cells.dx3;

  par_for_outer("divflux_x3", pmb->exe_space, scr_size, scr_level, js, je,
    KOKKOS_LAMBDA(TeamMember_t member, const int j)
    {
      AthenaScratch2D<Real> wl_flx(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> wr(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> wl_kp1(member.team_scratch(scr_level), nhydro_, ncells1);
      AthenaScratch2D<Real> uflux_km1(member.team_scratch(scr_level), nhydro_, ncells1);

      for (int k=ks-1; k<=ke+1; ++k) {
        // copy Wl from last iteration of k (unless this is the first time through)
        if (k>(ks-1)) { 
          wl_flx = wl_kp1;
        }

        switch (recon_method)
        {
          case ReconstructionMethod::dc:
            DonorCellX3(member, k, j, is, ie, w0_, wl_kp1, wr);
            break;
          case ReconstructionMethod::plm:
            PiecewiseLinearX3(member, k, j, is, ie, w0_, wl_kp1, wr);
            break;
          case ReconstructionMethod::ppm:
            PiecewiseParabolicX3(member, k, j, is, ie, w0_, wl_kp1, wr);
            break;
          default:
            break;
        }
        member.team_barrier();

        // compute fluxes over [ks,ke+1]
        if (k>(ks-1)) {
          switch (rsolver_method)
          {
            case RiemannSolver::advect:
              Advect(member, eos, is, ie, IVZ, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::llf:
              LLF(member, eos, is, ie, IVZ, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::hllc:
              HLLC(member, eos, is, ie, IVZ, wl_flx, wr, wl_flx);
              break;
            case RiemannSolver::roe:
              Roe(member, eos, is, ie, IVZ, wl_flx, wr, wl_flx);
              break;
            default:
              break;
          }
        }
        member.team_barrier();

        // Add dF/dx3
        // Fluxes must be summed together to symmetrize round-off error in each dir
        if (k>ks) {
          for (int n=0; n<nhydro_; ++n) {
            par_for_inner(member, is, ie, [&](const int i)
            { 
              divf_(n,k-1,j,i) += (wl_flx(n,i) - uflux_km1(n,i))/dx3;
            });
          }
        }
        member.team_barrier();

        // copy flux for use in next iteration
        if (k>(ks-1) && k<(ke+1)) {
          uflux_km1 = wl_flx;
        }
      } // end loop over k
    }
  );
  return TaskStatus::complete;
}

} // namespace hydro
