/*
 * SimpleTracker.cpp
 *
 *  Created on: Aug 11, 2016
 *      Author: hatem
 */

#include "SimpleTracker.h"
#include "MatrixOperations.h"
#include "PlanningHelpers.h"

#include <iostream>
#include <vector>
#include <cstdio>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>

namespace SimulationNS
{

using namespace PlannerHNS;

SimpleTracker::SimpleTracker()
{
	iTracksNumber = 1;
	m_Car.center.cost = 0;
	m_Car.l = 4.5;
	m_Car.w = 1.9;
	m_Car.id = 0;
	m_Car.t = CAR;
	m_MAX_ASSOCIATION_DISTANCE = 4.0;
	m_MAX_TRACKS_AFTER_LOSING = 25;
	m_bUseCenterOnly = false;
}

SimpleTracker::~SimpleTracker()
{

}

void SimpleTracker::DoOneStep(const WayPoint& currPose, const std::vector<DetectedObject>& obj_list)
{
	//2- Associate with previous detected Obstacles
	//	- Transform To Prev Car Pose
	//	- find closest previous detected object
	//	- assign track id
	//3- Apply Track Step using objects in car coordinate
	//4- Transfere back to Global Coordinate

	//Transform Previous scan
//	bool bEqual = false;
//	if(m_DetectedObjects.size() == obj_list.size())
//	{
//		for(unsigned int i = 0; i < obj_list.size(); i++)
//		{
//			if(obj_list.at(i).center.pos.x == m_DetectedObjects.at(i).center.pos.x
//					&& obj_list.at(i).center.pos.y == m_DetectedObjects.at(i).center.pos.y
//					&& obj_list.at(i).center.pos.a == m_DetectedObjects.at(i).center.pos.a)
//				bEqual = true;
//
//		}
//	}
	m_DetectedObjects = obj_list;
	m_Car.center = currPose;

	AssociateObjects();

	Track(m_DetectedObjects);

	m_PrevDetectedObjects = m_DetectedObjects;

	m_transform_ref.pos.x = m_PrevState.pos.x - currPose.pos.x;
	m_transform_ref.pos.y = m_PrevState.pos.y - currPose.pos.y;
	m_transform_ref.pos.a = m_PrevState.pos.a - currPose.pos.a;

	m_PrevState = currPose;

}

void SimpleTracker::AssociateObjects()
{
	std::vector<DetectedObject> hidden_list;
	DetectedObject* prev_obj;
	DetectedObject* curr_obj;
	DetectedObject curr_obj_transformed;


	for(unsigned int i = 0 ; i < m_DetectedObjects.size(); i++)
	{
		double minCost = 99999999;
		double minID = -1;

		curr_obj = &m_DetectedObjects.at(i);
		curr_obj->center.cost = 0;
		curr_obj_transformed = *curr_obj;
		CoordinateTransform(m_transform_ref, curr_obj_transformed);

		for(unsigned int j = 0; j < m_PrevDetectedObjects.size(); j++)
		{
			prev_obj = &m_PrevDetectedObjects.at(j);
			if(m_bUseCenterOnly)
			{
				//curr_obj->center.cost = distance2points(curr_obj->center.pos, curr_obj->center.pos);
				curr_obj->center.cost = distance2points(curr_obj_transformed.center.pos, curr_obj->center.pos);
			}
			else
			{
//				for(unsigned int k = 0; k < curr_obj->contour.size(); k++)
//					for(unsigned int pk = 0; pk < prev_obj->contour.size(); pk++)
//						curr_obj->center.cost += distance2points(curr_obj->contour.at(k), prev_obj->contour.at(pk));

				for(unsigned int k = 0; k < curr_obj_transformed.contour.size(); k++)
					for(unsigned int pk = 0; pk < prev_obj->contour.size(); pk++)
						curr_obj->center.cost += distance2points(curr_obj_transformed.contour.at(k), prev_obj->contour.at(pk));

				curr_obj->center.cost = curr_obj->center.cost/(double)(curr_obj->contour.size()*prev_obj->contour.size());
			}

			if(DEBUG_TRACKER)
				std::cout << "Cost Cost (" << i << "), " << prev_obj->center.pos.ToString() << ","
						<< curr_obj->center.cost <<  ", contour: " << curr_obj->contour.size()
						<< ", " << curr_obj->center.pos.ToString() << std::endl;

			if(curr_obj->center.cost < minCost)
			{
				minCost = curr_obj->center.cost;
				minID = prev_obj->id;
			}
		}

		if(minID <= 0 || minCost > m_MAX_ASSOCIATION_DISTANCE) // new Object enter the scene
		{
			iTracksNumber = iTracksNumber + 1;
			 curr_obj->id = iTracksNumber;
			if(DEBUG_TRACKER)
				std::cout << "New Matching " << curr_obj->id << ", "<< minCost<< ", " << iTracksNumber << std::endl;
		}
		else
		{
			 curr_obj->id = minID;
			if(DEBUG_TRACKER)
				std::cout << "Matched with ID  " << curr_obj->id << ", "<< minCost<< std::endl;
		}
	}

	for(unsigned int i = 0 ; i < m_PrevDetectedObjects.size(); i++)
	{
		prev_obj = &m_PrevDetectedObjects.at(i);
		bool bFound = false;
		for(unsigned int j = 0; j < m_DetectedObjects.size(); j++)
		{
			if(prev_obj->id == m_DetectedObjects.at(j).id)
			{
				bFound = true;
				break;
			}
		}

		if(!bFound && prev_obj->predicted_center.cost < m_MAX_TRACKS_AFTER_LOSING)
		{
			prev_obj->predicted_center.cost++;
			hidden_list.push_back(*prev_obj);
		}
	}

	m_DetectedObjects.insert(m_DetectedObjects.begin(), hidden_list.begin(), hidden_list.end());
}

void SimpleTracker::CreateTrack(DetectedObject& o)
{
	KFTrackV* pT = new KFTrackV(o.center.pos.x, o.center.pos.y,o.center.pos.a, o.id);
	o.id = pT->GetTrackID();
	pT->UpdateTracking(o.center.pos.x, o.center.pos.y, o.center.pos.a,
			o.center.pos.x, o.center.pos.y,o.center.pos.a, o.center.v);
	m_Tracks.push_back(pT);
}

KFTrackV* SimpleTracker::FindTrack(long index)
{
	for(unsigned int i=0; i< m_Tracks.size(); i++)
	{
		if(m_Tracks[i]->GetTrackID() == index)
			return m_Tracks[i];
	}

	return 0;
}

void SimpleTracker::Track(std::vector<DetectedObject>& objects_list)
{
	for(unsigned int i =0; i<objects_list.size(); i++)
	{
		if(objects_list[i].id >= 0)
		{
			KFTrackV* pT = FindTrack(objects_list[i].id);
			if(pT)
			{
				pT->UpdateTracking(objects_list[i].center.pos.x, objects_list[i].center.pos.y, objects_list[i].center.pos.a,
						objects_list[i].center.pos.x, objects_list[i].center.pos.y, objects_list[i].center.pos.a,
						objects_list[i].center.v);
				//std::cout << "Update Current Track " << objects_list[i].id << std::endl;
			}
			else
			{
				CreateTrack(objects_list[i]);
				//std::cout << "Create New Track " << objects_list[i].id << std::endl;
			}
		}
	}
}

void SimpleTracker::CoordinateTransform(const WayPoint& refCoordinate, DetectedObject& obj)
{
	Mat3 rotationMat(-refCoordinate.pos.a);
	Mat3 translationMat(-refCoordinate.pos.x, -refCoordinate.pos.y);
	obj.center.pos = translationMat*obj.center.pos;
	obj.center.pos = rotationMat*obj.center.pos;
	for(unsigned int j = 0 ; j < obj.contour.size(); j++)
	{
		obj.contour.at(j) = translationMat*obj.contour.at(j);
		obj.contour.at(j) = rotationMat*obj.contour.at(j);
	}
}

void SimpleTracker::CoordinateTransformPoint(const WayPoint& refCoordinate, GPSPoint& obj)
{
	Mat3 rotationMat(-refCoordinate.pos.a);
	Mat3 translationMat(-refCoordinate.pos.x, -refCoordinate.pos.y);
	obj = translationMat*obj;
	obj = rotationMat*obj;
}

} /* namespace BehaviorsNS */
