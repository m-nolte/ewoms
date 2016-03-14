// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 * \ingroup NcpModel
 *
 * \brief Declares the properties required for the NCP compositional
 *        multi-phase model.
 */
#ifndef EWOMS_NCP_PROPERTIES_HH
#define EWOMS_NCP_PROPERTIES_HH

#include <ewoms/models/common/multiphasebaseproperties.hh>
#include <ewoms/io/vtkcompositionmodule.hh>
#include <ewoms/io/vtkenergymodule.hh>
#include <ewoms/io/vtkdiffusionmodule.hh>

namespace Ewoms {
namespace Properties {
//! Enable the energy equation?
NEW_PROP_TAG(EnableEnergy);

//! Enable diffusive fluxes?
NEW_PROP_TAG(EnableDiffusion);

//! The unmodified weight for the pressure primary variable
NEW_PROP_TAG(NcpPressureBaseWeight);
//! The weight for the saturation primary variables
NEW_PROP_TAG(NcpSaturationsBaseWeight);
//! The unmodified weight for the fugacity primary variables
NEW_PROP_TAG(NcpFugacitiesBaseWeight);

//! The themodynamic constraint solver which calculates the
//! composition of any phase given all component fugacities.
NEW_PROP_TAG(NcpCompositionFromFugacitiesSolver);

//! Number of Newton iterations per time step where the update gets chopped?
NEW_PROP_TAG(NcpNewtonNumChoppedIterations);
} // namespace Properties
} // namespace Ewoms

#endif
