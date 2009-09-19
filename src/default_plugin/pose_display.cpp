/*
 * Copyright (c) 2008, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "pose_display.h"
#include "rviz/visualization_manager.h"
#include "rviz/properties/property.h"
#include "rviz/properties/property_manager.h"
#include "rviz/common.h"

#include "ogre_tools/arrow.h"
#include "ogre_tools/axes.h"

#include <tf/transform_listener.h>

#include <boost/bind.hpp>

#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSceneManager.h>

namespace rviz
{

PoseDisplay::PoseDisplay( const std::string& name, VisualizationManager* manager )
: Display( name, manager )
, color_( 1.0f, 0.1f, 0.0f )
, head_radius_(0.2)
, head_length_(0.3)
, shaft_radius_(0.1)
, shaft_length_(1.0)
, axes_length_(1.0)
, axes_radius_(0.1)
, tf_filter_(*manager->getTFClient(), "", 5, update_nh_)
{
  scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();

  tf_filter_.connectInput(sub_);
  tf_filter_.registerCallback(boost::bind(&PoseDisplay::incomingMessage, this, _1));

  arrow_ = new ogre_tools::Arrow(scene_manager_, scene_node_, shaft_length_, shaft_radius_, head_length_, head_radius_);

  axes_ = new ogre_tools::Axes(scene_manager_, scene_node_, axes_length_, axes_radius_);
  arrow_->getSceneNode()->setVisible(false);

  axes_->getSceneNode()->setVisible(false);

  Ogre::Quaternion quat(Ogre::Quaternion::IDENTITY);
  robotToOgre(quat);
  axes_->setOrientation(quat);

  setShape(Arrow);
  setAlpha(1.0);
}

PoseDisplay::~PoseDisplay()
{
  unsubscribe();

  clear();

  delete arrow_;
  delete axes_;
}

void PoseDisplay::clear()
{
  tf_filter_.clear();
}

void PoseDisplay::setTopic( const std::string& topic )
{
  unsubscribe();
  topic_ = topic;
  subscribe();

  propertyChanged(topic_property_);

  causeRender();
}

void PoseDisplay::setColor( const Color& color )
{
  color_ = color;

  arrow_->setColor(color.r_, color.g_, color.b_, alpha_);
  axes_->setColor(color.r_, color.g_, color.b_, alpha_);

  propertyChanged(color_property_);

  causeRender();
}

void PoseDisplay::setAlpha( float a )
{
  alpha_ = a;

  arrow_->setColor(color_.r_, color_.g_, color_.b_, alpha_);
  axes_->setColor(color_.r_, color_.g_, color_.b_, alpha_);

  propertyChanged(alpha_property_);

  causeRender();
}

void PoseDisplay::setHeadRadius(float r)
{
  head_radius_ = r;
  arrow_->set(shaft_length_, shaft_radius_, head_length_, head_radius_);
  propertyChanged(head_radius_property_);
}

void PoseDisplay::setHeadLength(float l)
{
  head_length_ = l;
  arrow_->set(shaft_length_, shaft_radius_, head_length_, head_radius_);
  propertyChanged(head_length_property_);
}

void PoseDisplay::setShaftRadius(float r)
{
  shaft_radius_ = r;
  arrow_->set(shaft_length_, shaft_radius_, head_length_, head_radius_);
  propertyChanged(shaft_radius_property_);
}

void PoseDisplay::setShaftLength(float l)
{
  shaft_length_ = l;
  arrow_->set(shaft_length_, shaft_radius_, head_length_, head_radius_);
  propertyChanged(shaft_length_property_);
}

void PoseDisplay::setAxesRadius(float r)
{
  axes_radius_ = r;
  axes_->set(axes_length_, axes_radius_);
  propertyChanged(axes_radius_property_);
}

void PoseDisplay::setAxesLength(float l)
{
  axes_length_ = l;
  axes_->set(axes_length_, axes_radius_);
  propertyChanged(axes_length_property_);
}

void PoseDisplay::setShape(int shape)
{
  current_shape_ = (Shape)shape;

  arrow_->getSceneNode()->setVisible(false);
  axes_->getSceneNode()->setVisible(false);

  switch (current_shape_)
  {
  case Arrow:
    arrow_->getSceneNode()->setVisible(true);
    break;
  case Axes:
    axes_->getSceneNode()->setVisible(true);
    break;
  }

  propertyChanged(shape_property_);

  createShapeProperties();

  causeRender();
}

void PoseDisplay::subscribe()
{
  if ( !isEnabled() )
  {
    return;
  }

  sub_.subscribe(update_nh_, topic_, 5);
}

void PoseDisplay::unsubscribe()
{
  sub_.unsubscribe();
}

void PoseDisplay::onEnable()
{
  //scene_node_->setVisible( true );
  switch (current_shape_)
  {
  case Arrow:
    arrow_->getSceneNode()->setVisible(true);
    break;
  case Axes:
    axes_->getSceneNode()->setVisible(true);
    break;
  }

  subscribe();
}

void PoseDisplay::onDisable()
{
  unsubscribe();
  clear();
  scene_node_->setVisible( false );
}

void PoseDisplay::createShapeProperties()
{
  if (!property_manager_)
  {
    return;
  }

  property_manager_->deleteProperty(shape_category_.lock());
  shape_category_ = property_manager_->createCategory("Shape Properties", property_prefix_, parent_category_, this);

  switch (current_shape_)
  {
  case Arrow:
    {
      color_property_ = property_manager_->createProperty<ColorProperty>( "Color", property_prefix_, boost::bind( &PoseDisplay::getColor, this ),
                                                                          boost::bind( &PoseDisplay::setColor, this, _1 ), shape_category_, this );
      alpha_property_ = property_manager_->createProperty<FloatProperty>( "Alpha", property_prefix_, boost::bind( &PoseDisplay::getAlpha, this ),
                                                                          boost::bind( &PoseDisplay::setAlpha, this, _1 ), shape_category_, this );
      FloatPropertyPtr float_prop = alpha_property_.lock();
      float_prop->setMin(0.0);
      float_prop->setMax(1.0);

      shaft_length_property_ = property_manager_->createProperty<FloatProperty>( "Shaft Length", property_prefix_, boost::bind( &PoseDisplay::getShaftLength, this ),
                                                                                 boost::bind( &PoseDisplay::setShaftLength, this, _1 ), shape_category_, this );
      shaft_radius_property_ = property_manager_->createProperty<FloatProperty>( "Shaft Radius", property_prefix_, boost::bind( &PoseDisplay::getShaftRadius, this ),
                                                                                 boost::bind( &PoseDisplay::setShaftRadius, this, _1 ), shape_category_, this );
      head_length_property_ = property_manager_->createProperty<FloatProperty>( "Head Length", property_prefix_, boost::bind( &PoseDisplay::getHeadLength, this ),
                                                                                  boost::bind( &PoseDisplay::setHeadLength, this, _1 ), shape_category_, this );
      head_radius_property_ = property_manager_->createProperty<FloatProperty>( "Head Radius", property_prefix_, boost::bind( &PoseDisplay::getHeadRadius, this ),
                                                                                 boost::bind( &PoseDisplay::setHeadRadius, this, _1 ), shape_category_, this );
    }
    break;
  case Axes:
    axes_length_property_ = property_manager_->createProperty<FloatProperty>( "Axes Length", property_prefix_, boost::bind( &PoseDisplay::getAxesLength, this ),
                                                                                boost::bind( &PoseDisplay::setAxesLength, this, _1 ), shape_category_, this );
    axes_radius_property_ = property_manager_->createProperty<FloatProperty>( "Axes Radius", property_prefix_, boost::bind( &PoseDisplay::getAxesRadius, this ),
                                                                               boost::bind( &PoseDisplay::setAxesRadius, this, _1 ), shape_category_, this );
    break;
  }

}

void PoseDisplay::createProperties()
{
  topic_property_ = property_manager_->createProperty<ROSTopicStringProperty>( "Topic", property_prefix_, boost::bind( &PoseDisplay::getTopic, this ),
                                                                                boost::bind( &PoseDisplay::setTopic, this, _1 ), parent_category_, this );
  ROSTopicStringPropertyPtr topic_prop = topic_property_.lock();
  topic_prop->setMessageType(geometry_msgs::PoseStamped::__s_getDataType());

  shape_property_ = property_manager_->createProperty<EnumProperty>( "Shape", property_prefix_, boost::bind( &PoseDisplay::getShape, this ),
                                                                     boost::bind( &PoseDisplay::setShape, this, _1 ), parent_category_, this );
  EnumPropertyPtr enum_prop = shape_property_.lock();
  enum_prop->addOption( "Arrow", Arrow );
  enum_prop->addOption( "Axes", Axes );

  createShapeProperties();
}

void PoseDisplay::targetFrameChanged()
{
}

void PoseDisplay::fixedFrameChanged()
{
  tf_filter_.setTargetFrame( fixed_frame_ );
  clear();
}

void PoseDisplay::update(float wall_dt, float ros_dt)
{
}

void PoseDisplay::incomingMessage( const geometry_msgs::PoseStamped::ConstPtr& message )
{
  std::string frame_id = message->header.frame_id;
  if ( frame_id.empty() )
  {
    frame_id = fixed_frame_;
  }

  btQuaternion bt_q;
  tf::quaternionMsgToTF(message->pose.orientation, bt_q);
  tf::Stamped<tf::Pose> pose;
  tf::poseStampedMsgToTF(*message, pose);

  try
  {
    vis_manager_->getTFClient()->transformPose( fixed_frame_, pose, pose );
  }
  catch(tf::TransformException& e)
  {
    ROS_ERROR( "Error transforming 2d base pose '%s' from frame '%s' to frame '%s'\n", name_.c_str(), message->header.frame_id.c_str(), fixed_frame_.c_str() );
  }

  btQuaternion quat;
  pose.getBasis().getRotation( quat );
  Ogre::Quaternion orient = Ogre::Quaternion::IDENTITY;
  ogreToRobot( orient );
  orient = Ogre::Quaternion( quat.w(), quat.x(), quat.y(), quat.z() ) * orient;
  robotToOgre(orient);
  scene_node_->setOrientation( orient );

  Ogre::Vector3 pos( pose.getOrigin().x(), pose.getOrigin().y(), pose.getOrigin().z() );
  robotToOgre( pos );
  scene_node_->setPosition( pos );

  causeRender();
}

void PoseDisplay::reset()
{
  clear();
}

} // namespace rviz
