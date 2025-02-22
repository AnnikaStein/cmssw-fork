#include "Geometry/RPCGeometry/interface/RPCGeometry.h"
#include "Geometry/CSCGeometry/interface/CSCGeometry.h"
#include "DataFormats/CSCRecHit/interface/CSCSegmentCollection.h"
#include "Geometry/CommonDetUnit/interface/GeomDet.h"
#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "Geometry/CommonTopologies/interface/TrapezoidalStripTopology.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "Geometry/RPCGeometry/interface/RPCGeomServ.h"
#include "DataFormats/RPCRecHit/interface/RPCRecHit.h"
#include "DataFormats/RPCRecHit/interface/RPCRecHitCollection.h"
#include "CSCSegtoRPC.h"
#include "CSCStationIndex.h"
#include "CSCObjectMap.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

CSCSegtoRPC::CSCSegtoRPC(edm::ConsumesCollector iC, const edm::ParameterSet& iConfig)
    : rpcGeoToken_(iC.esConsumes()), cscGeoToken_(iC.esConsumes()), cscMapToken_(iC.esConsumes()) {
  minBX = iConfig.getParameter<int>("minBX");
  maxBX = iConfig.getParameter<int>("maxBX");
}

std::unique_ptr<RPCRecHitCollection> CSCSegtoRPC::thePoints(const CSCSegmentCollection* allCSCSegments,
                                                            const edm::EventSetup& iSetup,
                                                            bool debug,
                                                            double eyr) {
  edm::ESHandle<RPCGeometry> rpcGeo = iSetup.getHandle(rpcGeoToken_);
  edm::ESHandle<CSCGeometry> cscGeo = iSetup.getHandle(cscGeoToken_);
  edm::ESHandle<CSCObjectMap> cscMap = iSetup.getHandle(cscMapToken_);

  double MaxD = 80.;

  if (debug)
    LogDebug("CSCSegtoRPC") << "CSC \t Number of CSC Segments in this event = " << allCSCSegments->size() << std::endl;

  auto _ThePoints = std::make_unique<RPCRecHitCollection>();
  edm::OwnVector<RPCRecHit> RPCPointVector;

  if (allCSCSegments->size() == 0) {
    if (debug)
      LogDebug("CSCSegtoRPC") << "CSC 0 segments skiping event" << std::endl;
  } else {
    std::map<CSCDetId, int> CSCSegmentsCounter;
    CSCSegmentCollection::const_iterator segment;

    int segmentsInThisEventInTheEndcap = 0;

    for (segment = allCSCSegments->begin(); segment != allCSCSegments->end(); ++segment) {
      CSCSegmentsCounter[segment->cscDetId()]++;
      segmentsInThisEventInTheEndcap++;
    }

    float myTime = -9999.;
    float myTimeErr = -9999.;
    int myBx = -99;
    const float bunchCrossTimeDiff = 25.;  // time between bunch crossings

    if (debug)
      LogDebug("CSCSegtoRPC") << "CSC \t loop over all the CSCSegments " << std::endl;
    for (segment = allCSCSegments->begin(); segment != allCSCSegments->end(); ++segment) {
      CSCDetId CSCId = segment->cscDetId();

      myTime = segment->time();
      myBx = round(myTime / bunchCrossTimeDiff);
      if (!(myBx <= maxBX && myBx >= minBX))  // rpc read in bx in [-2, 2]
        continue;

      if (debug)
        LogDebug("CSCSegtoRPC") << "CSC \t \t This Segment is in Chamber id: " << CSCId << std::endl;
      if (debug)
        LogDebug("CSCSegtoRPC") << "CSC \t \t Number of segments in this CSC = " << CSCSegmentsCounter[CSCId]
                                << std::endl;
      if (debug)
        LogDebug("CSCSegtoRPC")
            << "CSC \t \t Is the only one in this CSC? is not ind the ring 1? Are there more than 2 "
               "segments in the event?"
            << std::endl;

      if (CSCSegmentsCounter[CSCId] == 1 && CSCId.ring() != 1 && allCSCSegments->size() >= 2) {
        if (debug)
          LogDebug("CSCSegtoRPC") << "CSC \t \t yes" << std::endl;
        int cscEndCap = CSCId.endcap();
        int cscStation = CSCId.station();
        int cscRing = CSCId.ring();
        int rpcRegion = 1;
        if (cscEndCap == 2)
          rpcRegion = -1;  //Relacion entre las endcaps
        int rpcRing = cscRing;
        if (cscRing == 4)
          rpcRing = 1;
        int rpcStation = cscStation;
        int rpcSegment = CSCId.chamber();

        LocalPoint segmentPosition = segment->localPosition();
        LocalVector segmentDirection = segment->localDirection();
        float dz = segmentDirection.z();

        if (debug)
          LogDebug("CSCSegtoRPC") << "CSC \t \t \t Information about the segment"
                                  << "RecHits =" << segment->nRecHits() << "Angle =" << acos(dz) * 180 / 3.1415926
                                  << std::endl;

        if (debug)
          LogDebug("CSCSegtoRPC")
              << "CSC \t \t Is a good Segment? dim = 4, 4 <= nRecHits <= 10 Incident angle int range 45 < "
              << acos(dz) * 180 / 3.1415926 << " < 135? " << std::endl;

        if ((segment->dimension() == 4) && (segment->nRecHits() <= 10 && segment->nRecHits() >= 4)) {
          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t yes" << std::endl;
          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t CSC Segment Dimension " << segment->dimension() << std::endl;

          float Xo = segmentPosition.x();
          float Yo = segmentPosition.y();
          float Zo = segmentPosition.z();
          float dx = segmentDirection.x();
          float dy = segmentDirection.y();
          float dz = segmentDirection.z();

          if (debug)
            LogDebug("CSCSegtoRPC") << "Creating the CSCIndex" << std::endl;
          CSCStationIndex theindex(rpcRegion, rpcStation, rpcRing, rpcSegment);
          if (debug)
            LogDebug("CSCSegtoRPC") << "Getting the Rolls for the given index" << std::endl;
          std::set<RPCDetId> rollsForThisCSC = cscMap->getRolls(theindex);
          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t Getting chamber from Geometry" << std::endl;
          const CSCChamber* TheChamber = cscGeo->chamber(CSCId);
          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t Getting ID from Chamber" << std::endl;
          const CSCDetId TheId = TheChamber->id();

          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t Number of rolls for this CSC = " << rollsForThisCSC.size()
                                    << std::endl;

          if (debug)
            LogDebug("CSCSegtoRPC") << "CSC \t \t Printing The Id" << TheId << std::endl;

          if (rpcRing != 1) {  //They don't exist in Run3!

            assert(!rollsForThisCSC.empty());

            if (debug)
              LogDebug("CSCSegtoRPC") << "CSC \t \t Loop over all the rolls asociated to this CSC" << std::endl;
            for (std::set<RPCDetId>::iterator iteraRoll = rollsForThisCSC.begin(); iteraRoll != rollsForThisCSC.end();
                 iteraRoll++) {
              const RPCRoll* rollasociated = rpcGeo->roll(*iteraRoll);
              RPCDetId rpcId = rollasociated->id();

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t We are in the roll getting the surface" << rpcId << std::endl;
              const BoundPlane& RPCSurface = rollasociated->surface();

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t RollID: " << rpcId << std::endl;

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Doing the extrapolation to this roll" << std::endl;
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t CSC Segment Direction in CSCLocal " << segmentDirection
                                        << std::endl;
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t CSC Segment Point in CSCLocal " << segmentPosition
                                        << std::endl;

              GlobalPoint CenterPointRollGlobal = RPCSurface.toGlobal(LocalPoint(0, 0, 0));
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Center (0,0,0) of the Roll in Global" << CenterPointRollGlobal
                                        << std::endl;
              GlobalPoint CenterPointCSCGlobal = TheChamber->toGlobal(LocalPoint(0, 0, 0));
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Center (0,0,0) of the CSC in Global" << CenterPointCSCGlobal
                                        << std::endl;
              GlobalPoint segmentPositionInGlobal =
                  TheChamber->toGlobal(segmentPosition);  //new way to convert to global
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Segment Position in Global" << segmentPositionInGlobal
                                        << std::endl;
              LocalPoint CenterRollinCSCFrame = TheChamber->toLocal(CenterPointRollGlobal);

              if (debug) {  //to check CSC RPC phi relation!
                float rpcphi = 0;
                float cscphi = 0;

                (CenterPointRollGlobal.barePhi() < 0) ? rpcphi = 2 * 3.141592 + CenterPointRollGlobal.barePhi()
                                                      : rpcphi = CenterPointRollGlobal.barePhi();

                (CenterPointCSCGlobal.barePhi() < 0) ? cscphi = 2 * 3.1415926536 + CenterPointCSCGlobal.barePhi()
                                                     : cscphi = CenterPointCSCGlobal.barePhi();

                float df = fabs(cscphi - rpcphi);
                float dr = fabs(CenterPointRollGlobal.perp() - CenterPointCSCGlobal.perp());
                float diffz = CenterPointRollGlobal.z() - CenterPointCSCGlobal.z();
                float dfg = df * 180. / 3.14159265;

                if (debug)
                  LogDebug("CSCSegtoRPC") << "CSC \t \t \t z of RPC=" << CenterPointRollGlobal.z() << "z of CSC"
                                          << CenterPointCSCGlobal.z() << " dfg=" << dfg << std::endl;

                RPCGeomServ rpcsrv(rpcId);

                if (dr > 200. || fabs(dz) > 55. || dfg > 1.) {
                  if (debug)
                    LogDebug("CSCSegtoRPC")
                        << "\t \t \t CSC Station= " << CSCId.station() << " Ring= " << CSCId.ring()
                        << " Chamber= " << CSCId.chamber() << " cscphi=" << cscphi * 180 / 3.14159265
                        << "\t RPC Station= " << rpcId.station() << " ring= " << rpcId.ring() << " segment =-> "
                        << rpcsrv.segment() << " rollphi=" << rpcphi * 180 / 3.14159265 << "\t dfg=" << dfg
                        << " dz=" << diffz << " dr=" << dr << std::endl;
                }
              }

              float D = CenterRollinCSCFrame.z();

              float X = Xo + dx * D / dz;
              float Y = Yo + dy * D / dz;
              float Z = D;

              const TrapezoidalStripTopology* top_ =
                  dynamic_cast<const TrapezoidalStripTopology*>(&(rollasociated->topology()));
              LocalPoint xmin = top_->localPosition(0.);
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t xmin of this  Roll " << xmin << "cm" << std::endl;
              LocalPoint xmax = top_->localPosition((float)rollasociated->nstrips());
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t xmax of this  Roll " << xmax << "cm" << std::endl;
              float rsize = fabs(xmax.x() - xmin.x());
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Roll Size " << rsize << "cm" << std::endl;
              float stripl = top_->stripLength();
              float stripw = top_->pitch();

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Strip Lenght " << stripl << "cm" << std::endl;
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Strip Width " << stripw << "cm" << std::endl;

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t X Predicted in CSCLocal= " << X << "cm" << std::endl;
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Y Predicted in CSCLocal= " << Y << "cm" << std::endl;
              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Z Predicted in CSCLocal= " << Z << "cm" << std::endl;

              float extrapolatedDistance = sqrt((X - Xo) * (X - Xo) + (Y - Yo) * (Y - Yo) + (Z - Zo) * (Z - Zo));

              if (debug)
                LogDebug("CSCSegtoRPC") << "CSC \t \t \t Is the distance of extrapolation less than MaxD? ="
                                        << extrapolatedDistance << "cm"
                                        << " MaxD=" << MaxD << "cm" << std::endl;

              if (extrapolatedDistance <= MaxD) {
                if (debug)
                  LogDebug("CSCSegtoRPC") << "CSC \t \t \t yes" << std::endl;

                GlobalPoint GlobalPointExtrapolated = TheChamber->toGlobal(LocalPoint(X, Y, Z));
                if (debug)
                  LogDebug("CSCSegtoRPC")
                      << "CSC \t \t \t Point ExtraPolated in Global" << GlobalPointExtrapolated << std::endl;

                LocalPoint PointExtrapolatedRPCFrame = RPCSurface.toLocal(GlobalPointExtrapolated);

                if (debug)
                  LogDebug("CSCSegtoRPC")
                      << "CSC \t \t \t Point Extrapolated in RPCLocal" << PointExtrapolatedRPCFrame << std::endl;
                if (debug)
                  LogDebug("CSCSegtoRPC") << "CSC \t \t \t Corner of the Roll = (" << rsize * eyr << "," << stripl * eyr
                                          << ")" << std::endl;
                if (debug)
                  LogDebug("CSCSegtoRPC")
                      << "CSC \t \t \t Info About the Point Extrapolated in X Abs ("
                      << fabs(PointExtrapolatedRPCFrame.x()) << "," << fabs(PointExtrapolatedRPCFrame.y()) << ","
                      << fabs(PointExtrapolatedRPCFrame.z()) << ")" << std::endl;
                if (debug)
                  LogDebug("CSCSegtoRPC") << "CSC \t \t \t dz=" << fabs(PointExtrapolatedRPCFrame.z())
                                          << " dx=" << fabs(PointExtrapolatedRPCFrame.x())
                                          << " dy=" << fabs(PointExtrapolatedRPCFrame.y()) << std::endl;

                if (debug)
                  LogDebug("CSCSegtoRPC") << "CSC \t \t \t Does the extrapolation go inside this roll????" << std::endl;

                if (fabs(PointExtrapolatedRPCFrame.z()) < 1. && fabs(PointExtrapolatedRPCFrame.x()) < rsize * eyr &&
                    fabs(PointExtrapolatedRPCFrame.y()) < stripl * eyr) {
                  if (debug)
                    LogDebug("CSCSegtoRPC") << "CSC \t \t \t \t yes" << std::endl;
                  if (debug)
                    LogDebug("CSCSegtoRPC") << "CSC \t \t \t \t Creating the RecHit" << std::endl;

                  RPCRecHit RPCPoint(rpcId, myBx, PointExtrapolatedRPCFrame);
                  RPCPoint.setTimeAndError(myTime, myTimeErr);

                  if (debug)
                    LogDebug("CSCSegtoRPC") << "CSC \t \t \t \t Clearing the vector" << std::endl;
                  RPCPointVector.clear();
                  if (debug)
                    LogDebug("CSCSegtoRPC") << "CSC \t \t \t \t Pushing back" << std::endl;
                  RPCPointVector.push_back(RPCPoint);
                  if (debug)
                    LogDebug("CSCSegtoRPC") << "CSC \t \t \t \t Putting the vector" << std::endl;
                  _ThePoints->put(rpcId, RPCPointVector.begin(), RPCPointVector.end());
                }
              }
            }
          }
        }
      }
    }
  }
  return _ThePoints;
}
