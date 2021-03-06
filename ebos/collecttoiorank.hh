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
#ifndef EWOMS_PARALLELSERIALOUTPUT_HH
#define EWOMS_PARALLELSERIALOUTPUT_HH

#include <opm/output/data/Cells.hpp>
#include <opm/output/data/Solution.hpp>

//#if HAVE_OPM_GRID
#include <dune/grid/common/p2pcommunicator.hh>
#include <dune/grid/utility/persistentcontainer.hh>
#include <dune/grid/common/gridenums.hh>
//#else
//#error "This header needs the opm-grid module."
//#endif

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>

#include <opm/common/ErrorMacros.hpp>
#include <opm/common/Exceptions.hpp>

#include <dune/grid/common/mcmgmapper.hh>

#include <stdexcept>

namespace Ewoms
{
    template < class GridManager >
    class CollectDataToIORank
    {
    public:
        typedef typename GridManager :: Grid  Grid;
        typedef typename Grid :: CollectiveCommunication  CollectiveCommunication;

        // global id
        class GlobalCellIndex
        {
            int globalId_;
            int localIndex_;
            bool isInterior_;
        public:
            GlobalCellIndex() : globalId_(-1), localIndex_(-1), isInterior_(true) {}
            void setGhost() { isInterior_ = false; }

            void setId( const int globalId ) { globalId_ = globalId; }
            void setIndex( const int localIndex ) { localIndex_ = localIndex; }

            int localIndex () const { return localIndex_; }
            int id () const { return globalId_; }
            bool isInterior() const { return isInterior_; }
        };

        typedef typename Dune::PersistentContainer< Grid, GlobalCellIndex > GlobalIndexContainer;

        static const int dimension = Grid :: dimension ;

        typedef typename Grid :: LeafGridView             GridView;
        typedef GridView                                  AllGridView;

        typedef Dune :: Point2PointCommunicator< Dune :: SimpleMessageBuffer > P2PCommunicatorType;
        typedef typename P2PCommunicatorType :: MessageBufferType MessageBufferType;

        typedef std::vector< GlobalCellIndex > LocalIndexMapType;

        typedef std::vector<int> IndexMapType;
        typedef std::vector< IndexMapType > IndexMapStorageType;

        class DistributeIndexMapping : public P2PCommunicatorType::DataHandleInterface
        {
        protected:
            const std::vector<int>& distributedGlobalIndex_;
            IndexMapType& localIndexMap_;
            IndexMapStorageType& indexMaps_;
            std::map< const int, const int > globalPosition_;

        public:
            DistributeIndexMapping( const std::vector<int>& globalIndex,
                                    const std::vector<int>& distributedGlobalIndex,
                                    IndexMapType& localIndexMap,
                                    IndexMapStorageType& indexMaps )
            : distributedGlobalIndex_( distributedGlobalIndex ),
              localIndexMap_( localIndexMap ),
              indexMaps_( indexMaps ),
              globalPosition_()
            {
                const size_t size = globalIndex.size();
                // create mapping globalIndex --> localIndex
                for ( size_t index = 0; index < size; ++index )
                {
                    globalPosition_.insert( std::make_pair( globalIndex[ index ], index ) );
                }

                // we need to create a mapping from local to global
                if( ! indexMaps_.empty() )
                {
                    // for the ioRank create a localIndex to index in global state map
                    IndexMapType& indexMap = indexMaps_.back();
                    const size_t localSize = localIndexMap_.size();
                    indexMap.resize( localSize );
                    for( size_t i=0; i<localSize; ++i )
                    {
                        const int id = distributedGlobalIndex_[ localIndexMap_[ i ] ];
                        indexMap[ i ] = globalPosition_[ id ] ;
                    }
                }
            }

            void pack( const int link, MessageBufferType& buffer )
            {
                // we should only get one link
                if( link != 0 ) {
                    OPM_THROW(std::logic_error,"link in method pack is not 0 as execpted");
                }

                // pack all interior global cell id's
                const int size = localIndexMap_.size();
                buffer.write( size );

                for( int index = 0; index < size; ++index )
                {
                    const int globalIdx = distributedGlobalIndex_[ localIndexMap_[ index ] ];
                    buffer.write( globalIdx );
                }
            }

            void unpack( const int link, MessageBufferType& buffer )
            {
                // get index map for current link
                IndexMapType& indexMap = indexMaps_[ link ];
                assert( ! globalPosition_.empty() );

                // unpack all interior global cell id's
                int numCells = 0;
                buffer.read( numCells );
                indexMap.resize( numCells );
                for( int index = 0; index < numCells; ++index )
                {
                    int globalId = -1;
                    buffer.read( globalId );
                    assert( globalPosition_.find( globalId ) != globalPosition_.end() );
                    indexMap[ index ] = globalPosition_[ globalId ];
                }
            }
        };

        enum { ioRank = 0 };

        static const bool needsReordering = ! std::is_same<
            typename GridManager::Grid, typename GridManager::EquilGrid > :: value ;

        CollectDataToIORank( const GridManager& gridManager )
            : toIORankComm_( )
        {
            // index maps only have to be build when reordering is needed
            if( ! needsReordering && ! isParallel() )
            {
                return ;
            }

            const CollectiveCommunication& comm = gridManager.grid().comm();

            {
                std::set< int > send, recv;
		typedef typename GridManager::EquilGrid::LeafGridView EquilGridView;
                const EquilGridView equilGridView = gridManager.equilGrid().leafGridView() ;

#if DUNE_VERSION_NEWER(DUNE_GRID, 2,6)
                typedef Dune::MultipleCodimMultipleGeomTypeMapper<EquilGridView> EquilElementMapper;
                EquilElementMapper equilElemMapper(equilGridView, Dune::mcmgElementLayout());
#else
                typedef Dune::MultipleCodimMultipleGeomTypeMapper<EquilGridView, Dune::MCMGElementLayout> EquilElementMapper;
                EquilElementMapper equilElemMapper(equilGridView);
#endif

                // We need a mapping from local to global grid, here we
                // use equilGrid which represents a view on the global grid
                const size_t globalSize = gridManager.equilGrid().leafGridView().size( 0 );
                // reserve memory
                globalCartesianIndex_.resize(globalSize, -1);

                // loop over all elements (global grid) and store Cartesian index
                auto elemIt = gridManager.equilGrid().leafGridView().template begin<0>();
                const auto& elemEndIt = gridManager.equilGrid().leafGridView().template end<0>();
                for (; elemIt != elemEndIt; ++elemIt) {
                    int elemIdx = equilElemMapper.index(*elemIt );
                    int cartElemIdx = gridManager.equilCartesianIndexMapper().cartesianIndex(elemIdx);
                    globalCartesianIndex_[elemIdx] = cartElemIdx;
                }

                // the I/O rank receives from all other ranks
                if( isIORank() )
                {
                    for(int i=0; i<comm.size(); ++i)
                    {
                        if( i != ioRank )
                        {
                            recv.insert( i );
                        }
                    }
                }
                else // all other simply send to the I/O rank
                {
                    send.insert( ioRank );
                }

                localIndexMap_.clear();
                const size_t gridSize = gridManager.grid().size( 0 );
                localIndexMap_.reserve( gridSize );

                // store the local Cartesian index
                IndexMapType distributedCartesianIndex;
                distributedCartesianIndex.resize(gridSize, -1);

                typedef typename GridManager::GridView LocalGridView;
                const LocalGridView localGridView = gridManager.gridView() ;

#if DUNE_VERSION_NEWER(DUNE_GRID, 2,6)
                typedef Dune::MultipleCodimMultipleGeomTypeMapper<LocalGridView> ElementMapper;
                ElementMapper elemMapper(localGridView, Dune::mcmgElementLayout());
#else
                typedef Dune::MultipleCodimMultipleGeomTypeMapper<LocalGridView, Dune::MCMGElementLayout> ElementMapper;
                ElementMapper elemMapper(localGridView);
#endif

                // A mapping for the whole grid (including the ghosts) is needed for restarts
                for( auto it = localGridView.template begin< 0 >(),
                     end = localGridView.template end< 0 >(); it != end; ++it )
                {
                    const auto element = *it ;
                    int elemIdx = elemMapper.index( element );
                    distributedCartesianIndex[elemIdx] = gridManager.cartesianIndex( elemIdx );

                    // only store interior element for collection
                    //assert( element.partitionType() == Dune :: InteriorEntity );

                    localIndexMap_.push_back( elemIdx );
                }

                // insert send and recv linkage to communicator
                toIORankComm_.insertRequest( send, recv );

                // need an index map for each rank
                indexMaps_.clear();
                indexMaps_.resize( comm.size() );

                // distribute global id's to io rank for later association of dof's
                DistributeIndexMapping distIndexMapping( globalCartesianIndex_, distributedCartesianIndex, localIndexMap_, indexMaps_ );
                toIORankComm_.exchange( distIndexMapping );
            }
        }

        class PackUnPack : public P2PCommunicatorType::DataHandleInterface
        {
            const Opm::data::Solution& localCellData_;
            Opm::data::Solution& globalCellData_;

            const IndexMapType& localIndexMap_;
            const IndexMapStorageType& indexMaps_;

        public:
            PackUnPack( const Opm::data::Solution& localCellData,
                        Opm::data::Solution& globalCellData,
                        const IndexMapType& localIndexMap,
                        const IndexMapStorageType& indexMaps,
                        const size_t globalSize,
                        const bool isIORank )
            : localCellData_( localCellData ),
              globalCellData_( globalCellData ),
              localIndexMap_( localIndexMap ),
              indexMaps_( indexMaps )
            {
                if( isIORank )
                {
                    // add missing data to global cell data
                    for (const auto& pair : localCellData_) {
                        const std::string& key = pair.first;
                        std::size_t container_size = globalSize;
                        auto OPM_OPTIM_UNUSED ret = globalCellData_.insert(key, pair.second.dim,
                                                                           std::vector<double>(container_size),
                                                                           pair.second.target);
                        assert(ret.second);
                    }

                    MessageBufferType buffer;
                    pack( 0, buffer );

                    // the last index map is the local one
                    doUnpack( indexMaps.back(), buffer );
                }
            }

            // pack all data associated with link
            void pack( const int link, MessageBufferType& buffer )
            {
                // we should only get one link
                if( link != 0 ) {
                    OPM_THROW(std::logic_error,"link in method pack is not 0 as expected");
                }

                // write all cell data registered in local state
                for (const auto& pair : localCellData_) {
                    const auto& data = pair.second.data;

                    // write all data from local data to buffer
                    write( buffer, localIndexMap_, data);
                }

            }

            void doUnpack( const IndexMapType& indexMap, MessageBufferType& buffer )
            {
                // we loop over the data  as
                // its order governs the order the data got received.
                for (auto& pair : localCellData_) {
                    const std::string& key = pair.first;
                    auto& data = globalCellData_.data(key);

                    //write all data from local cell data to buffer
                    read( buffer, indexMap, data);
                }
            }

            // unpack all data associated with link
            void unpack( const int link, MessageBufferType& buffer )
            {
                doUnpack( indexMaps_[ link ], buffer );
            }

        protected:
            template <class Vector>
            void write( MessageBufferType& buffer,
                        const IndexMapType& localIndexMap,
                        const Vector& vector,
                        const unsigned int offset = 0,
                        const unsigned int stride = 1 ) const
            {
                unsigned int size = localIndexMap.size();
                buffer.write( size );
                assert( vector.size() >= stride * size );
                for( unsigned int i=0; i<size; ++i )
                {
                    const unsigned int index = localIndexMap[ i ] * stride + offset;
                    assert( index < vector.size() );
                    buffer.write( vector[ index ] );
                }
            }

            template <class Vector>
            void read( MessageBufferType& buffer,
                       const IndexMapType& indexMap,
                       Vector& vector,
                       const unsigned int offset = 0, const unsigned int stride = 1 ) const
            {
                unsigned int size = 0;
                buffer.read( size );
                assert( size == indexMap.size() );
                for( unsigned int i=0; i<size; ++i )
                {
                    const unsigned int index = indexMap[ i ] * stride + offset;
                    assert( index < vector.size() );
                    buffer.read( vector[ index ] );
                }
            }


        };

        // gather solution to rank 0 for EclipseWriter
        void collect( const Opm::data::Solution& localCellData )
        {
            globalCellData_ = {};
            // index maps only have to be build when reordering is needed
            if( ! needsReordering && ! isParallel() )
            {
                return ;
            }

            // this also packs and unpacks the local buffers one ioRank
            PackUnPack
                packUnpack( localCellData,
                            globalCellData_,
                            localIndexMap_,
                            indexMaps_,
                            numCells(),
                            isIORank() );

            if ( ! isParallel() )
            {
                // no need to collect anything.
                return;
            }

            //toIORankComm_.exchangeCached( packUnpack );
            toIORankComm_.exchange( packUnpack );

#ifndef NDEBUG
            // mkae sure every process is on the same page
            toIORankComm_.barrier();
#endif
        }

        const Opm::data::Solution& globalCellData() const
        {
            return globalCellData_;
        }

        bool isIORank() const
        {
            return toIORankComm_.rank() == ioRank;
        }

        bool isParallel() const
        {
            return toIORankComm_.size() > 1;
        }

        int localIdxToGlobalIdx(const unsigned localIdx) {

            if ( ! isParallel() )
            {
                return localIdx;
            }
            // the last indexMap is the local one
            IndexMapType& indexMap = indexMaps_.back();
            if( indexMap.empty() )
                OPM_THROW(std::logic_error,"index map is not created on this rank");

            if (localIdx > indexMap.size())
                OPM_THROW(std::logic_error,"local index is outside map range");

            return indexMap[localIdx];
        }

        size_t numCells () const { return globalCartesianIndex_.size(); }

    protected:
        P2PCommunicatorType             toIORankComm_;
        IndexMapType                    globalCartesianIndex_;
        IndexMapType                    localIndexMap_;
        IndexMapStorageType             indexMaps_;
        Opm::data::Solution             globalCellData_;
    };

} // end namespace Opm
#endif
