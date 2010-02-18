// $Id$
/*****************************************************************************
 *   Copyright (C) 2008-2009 by Melanie Darcis 								 *
 *   Copyright (C) 2008-2010 by Andreas Lauser                               *
 *   Copyright (C) 2008-2009 by Klaus Mosthaf                                *
 *   Copyright (C) 2008-2009 by Bernd Flemisch                               *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
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
 * \brief Contains the quantities which are constant within a
 *        finite volume in the non-isothermal two-phase, two-component
 *        model.
 */
#ifndef DUMUX_2P2CNI_VERTEX_DATA_HH
#define DUMUX_2P2CNI_VERTEX_DATA_HH

#include <dumux/boxmodels/2p2c/2p2cvertexdata.hh>

namespace Dune
{

/*!
 * \ingroup TwoPTwoCNIBoxModel
 * \brief Contains the quantities which are are constant within a
 *        finite volume in the non-isothermal two-phase, two-component
 *        model.
 */
template <class TypeTag>
class TwoPTwoCNIVertexData : public TwoPTwoCVertexData<TypeTag>
{
    typedef TwoPTwoCVertexData<TypeTag> ParentType;

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar))   Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView)) GridView;

    typedef typename GridView::template Codim<0>::Entity Element;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FVElementGeometry)) FVElementGeometry;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Problem)) Problem;

    enum {
        dim           = GridView::dimension,
        dimWorld      = GridView::dimensionworld,
    };

    typedef typename GET_PROP_TYPE(TypeTag, PTAG(FluidSystem))     FluidSystem;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(TwoPTwoCIndices)) Indices;
    enum { numPhases = GET_PROP_VALUE(TypeTag, PTAG(NumPhases)) };
    enum { numComponents = GET_PROP_VALUE(TypeTag, PTAG(NumComponents)) };
    enum { temperatureIdx = Indices::temperatureIdx };

    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes))                SolutionTypes;
    typedef typename GET_PROP(TypeTag, PTAG(ReferenceElements))::Container ReferenceElements;

    typedef typename SolutionTypes::PrimaryVarVector  PrimaryVarVector;
    typedef Dune::FieldVector<Scalar, numPhases>      PhasesVector;

public:
    /*!
     * \brief Update all quantities for a given control volume.
     */
    void update(const PrimaryVarVector  &sol,
                const Element           &element,
                const FVElementGeometry &elemGeom,
                int                      vertIdx,
                Problem                 &problem,
                bool                     isOldSol) 
    {
        // vertex update data for the mass balance
        ParentType::update(sol,
                           element,
                           elemGeom,
                           vertIdx,
                           problem,
                           isOldSol);

        // the internal energies and the enthalpies
        for (int phaseIdx = 0; phaseIdx < numPhases; ++phaseIdx) {
            enthalpy_[phaseIdx] = 
                FluidSystem::enthalpy(phaseIdx, 
                                      this->phaseState());
            internalEnergy_[phaseIdx] =
                FluidSystem::internalEnergy(phaseIdx, 
                                            this->phaseState());
        }
        Valgrind::CheckDefined(internalEnergy_);
        Valgrind::CheckDefined(enthalpy_);
    };

    // this method gets called by the parent class
    void updateTemperature_(const PrimaryVarVector  &sol,
                            const Element           &element,
                            const FVElementGeometry &elemGeom,
                            int                      scvIdx,
                            const Problem           &problem)
    {
        // retrieve temperature from solution vector
        this->temperature_ = sol[temperatureIdx];

        heatCapacity_ =
            problem.spatialParameters().heatCapacity(element, elemGeom, scvIdx);

        Valgrind::CheckDefined(this->temperature_);
        Valgrind::CheckDefined(heatCapacity_);
    }

    /*!
     * \brief Returns the total internal energy of a phase in the
     *        sub-control volume.
     */
    Scalar internalEnergy(int phaseIdx) const
    { return internalEnergy_[phaseIdx]; };

    /*!
     * \brief Returns the total enthalpy of a phase in the sub-control
     *        volume.
     */
    Scalar enthalpy(int phaseIdx) const
    { return enthalpy_[phaseIdx]; };

    /*!
     * \brief Returns the total heat capacity [J/(K m^3)] of the rock matrix in
     *        the sub-control volume.
     */
    Scalar heatCapacity() const
    { return heatCapacity_; };
    
protected:
    Scalar internalEnergy_[numPhases];
    Scalar enthalpy_[numPhases];
    Scalar heatCapacity_;
};

} // end namepace

#endif
