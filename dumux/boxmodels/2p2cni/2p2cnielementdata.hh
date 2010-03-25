// $Id$
/*****************************************************************************
 *   Copyright (C) 2009 by Andreas Lauser                                    *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version, as long as this copyright notice    *
 *   is included in its original form.                                       *
 *                                                                           *
 *   This program is distributed WITHOUT ANY WARRANTY.                       *
 *****************************************************************************/
/*!
 * \file
 *
 * \brief This template class contains the quantities which are
 *        constant within a finite element in the non-isothermal
 *        two-phase, two-component model.
 */
#ifndef DUMUX_2P2CNI_ELEMENT_DATA_HH
#define DUMUX_2P2CNI_ELEMENT_DATA_HH

#include <dumux/boxmodels/2p2c/2p2celementdata.hh>

namespace Dumux
{

/*!
 * \ingroup TwoPTwoCNIBoxModel
 * \brief This template class contains the quantities which are
 *        constant within a finite element in the non-isothermal
 *        two-phase, two-component model.
 *
 * For the plain non-isothermal two-phase two-component model everything is given on the finite
 * volumes, so this class is empty.
 */
template <class TypeTag>
class TwoPTwoCNIElementData : public TwoPTwoCElementData<TypeTag>
{
};

} // end namepace

#endif
