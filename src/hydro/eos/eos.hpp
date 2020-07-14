#ifndef HYDRO_EOS_EOS_HPP_
#define HYDRO_EOS_EOS_HPP_
//========================================================================================
// AthenaXXX astrophysical plasma code
// Copyright(C) 2020 James M. Stone <jmstone@ias.edu> and the Athena code team
// Licensed under the 3-clause BSD License (the "LICENSE")
//========================================================================================
//! \file eos.hpp
//  \brief defines class EquationOfState
//  Contains data and functions that implement conserved<->primitive variables conversion

#include "athena.hpp"
#include "athena_arrays.hpp"
#include "mesh/meshblock.hpp"
#include "parameter_input.hpp"

namespace hydro {

//----------------------------------------------------------------------------------------
//! \class EquationOfState
//  \brief abstract base class for all EOS classes

class EquationOfState {
 public:
  EquationOfState(Hydro *phyd, std::unique_ptr<ParameterInput> &pin);
  virtual ~EquationOfState() = default;

  Hydro *pmy_hydro;

  virtual void ConservedToPrimitive(const int k, const int j, const int il,const  int iu, 
    AthenaCenterArray<Real> &cons, AthenaCenterArray<Real> &prim) = 0;

 protected:
  Real density_floor_, pressure_floor_;
};

//----------------------------------------------------------------------------------------
//! \class Adiabatic Hydro
//  \brief derived EOS class for adiabatic hydrodynamics

class AdiabaticHydro : public EquationOfState {
 public:
  AdiabaticHydro(Hydro *phyd, std::unique_ptr<ParameterInput> &pin);

  Real GetGamma() const {return gamma_;}

  void ConservedToPrimitive(const int k, const int j, const int il,const  int iu, 
    AthenaCenterArray<Real> &cons, AthenaCenterArray<Real> &prim) override;

 private:
  Real gamma_;
};

} // namespace hydro

#endif // HYDRO_EOS_EOS_HPP_