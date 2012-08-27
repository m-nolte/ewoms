// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2012 by Andreas Lauser                                    *
 *   Copyright (C) 2012 by Bernd Flemisch                                    *
 *   Copyright (C) 2012 by Melanie Darcis                                    *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/

/*!
 * \file
 *
 * \brief Defines the indices required for the PVS BOX model.
 */
#ifndef DUMUX_PVS_INDICES_HH
#define DUMUX_PVS_INDICES_HH

#include "pvsproperties.hh"
#include <dumux/boxmodels/modules/energy/boxmultiphaseenergymodule.hh>

namespace Dumux
{
// \{

/*!
 * \brief The indices for the isothermal PVS model.
 *
 * \tparam PVOffset The first index in a primary variable vector.
 */
template <class TypeTag, int PVOffset>
class PvsIndices
    : public BoxMultiPhaseEnergyIndices<PVOffset + GET_PROP_VALUE(TypeTag, NumComponents), GET_PROP_VALUE(TypeTag, EnableEnergy)>
{
    enum { numComponents = GET_PROP_VALUE(TypeTag, NumComponents) };
    enum { enableEnergy = GET_PROP_VALUE(TypeTag, EnableEnergy) };
    typedef BoxMultiPhaseEnergyIndices<PVOffset + numComponents, enableEnergy> EnergyIndices;
public:
    // number of equations/primary variables
    static const int numEq = numComponents + EnergyIndices::numEq_;

    // Primary variable indices
    static const int pressure0Idx = PVOffset + 0; //!< Index for the pressure of the first phase
    static const int switch0Idx = PVOffset + 1; //!< Index of the either the saturation or the mole fraction of the phase with the lowest index

    // equation indices
    static const int conti0EqIdx = PVOffset; //!< Index of the mass conservation equation for the first component
};

// \}

}

#endif
