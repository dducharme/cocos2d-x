/****************************************************************************
 Copyright (c) 2013 cocos2d-x.org
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "CCPhysicsWorld.h"
#ifdef CC_USE_PHYSICS

#if (CC_PHYSICS_ENGINE == CC_PHYSICS_CHIPMUNK)
#include "chipmunk.h"
#elif (CC_PHYSICS_ENGINE == CCPHYSICS_BOX2D)
#include "Box2D.h"
#endif

#include "CCPhysicsBody.h"
#include "CCPhysicsShape.h"
#include "CCPhysicsContact.h"
#include "CCPhysicsJoint.h"
#include "CCPhysicsContact.h"

#include "chipmunk/CCPhysicsWorldInfo.h"
#include "box2d/CCPhysicsWorldInfo.h"
#include "chipmunk/CCPhysicsBodyInfo.h"
#include "box2d/CCPhysicsBodyInfo.h"
#include "chipmunk/CCPhysicsShapeInfo.h"
#include "box2d/CCPhysicsShapeInfo.h"
#include "chipmunk/CCPhysicsContactInfo.h"
#include "box2d/CCPhysicsContactInfo.h"
#include "chipmunk/CCPhysicsJointInfo.h"
#include "box2d/CCPhysicsJointInfo.h"
#include "chipmunk/CCPhysicsHelper.h"

#include "CCDrawNode.h"
#include "CCArray.h"
#include "CCScene.h"
#include "CCDirector.h"

#include <algorithm>

NS_CC_BEGIN

#if (CC_PHYSICS_ENGINE == CC_PHYSICS_CHIPMUNK)

const float PHYSICS_INFINITY = INFINITY;

int PhysicsWorld::collisionBeginCallbackFunc(cpArbiter *arb, struct cpSpace *space, void *data)
{
    PhysicsWorld* world = static_cast<PhysicsWorld*>(data);
    
    CP_ARBITER_GET_SHAPES(arb, a, b);
    
    auto ita = PhysicsShapeInfo::map.find(a);
    auto itb = PhysicsShapeInfo::map.find(b);
    CC_ASSERT(ita != PhysicsShapeInfo::map.end() && itb != PhysicsShapeInfo::map.end());
    
    PhysicsContact* contact = PhysicsContact::create(ita->second->shape, itb->second->shape);
    arb->data = contact;
    
    return world->collisionBeginCallback(*static_cast<PhysicsContact*>(arb->data));
}

int PhysicsWorld::collisionPreSolveCallbackFunc(cpArbiter *arb, cpSpace *space, void *data)
{
    PhysicsWorld* world = static_cast<PhysicsWorld*>(data);
    return world->collisionPreSolveCallback(*static_cast<PhysicsContact*>(arb->data),
                                            PhysicsContactPreSolve());
}

void PhysicsWorld::collisionPostSolveCallbackFunc(cpArbiter *arb, cpSpace *space, void *data)
{
    PhysicsWorld* world = static_cast<PhysicsWorld*>(data);
    world->collisionPostSolveCallback(*static_cast<PhysicsContact*>(arb->data),
                                      PhysicsContactPostSolve());
}

void PhysicsWorld::collisionSeparateCallbackFunc(cpArbiter *arb, cpSpace *space, void *data)
{
    PhysicsWorld* world = static_cast<PhysicsWorld*>(data);
    PhysicsContact* contact = static_cast<PhysicsContact*>(arb->data);
    
    world->collisionSeparateCallback(*contact);
    
    delete contact;
}

bool PhysicsWorld::init()
{
    _info = new PhysicsWorldInfo();
    
    cpSpaceSetGravity(_info->space, PhysicsHelper::point2cpv(_gravity));
    
    cpSpaceSetDefaultCollisionHandler(_info->space,
                                      PhysicsWorld::collisionBeginCallbackFunc,
                                      PhysicsWorld::collisionPreSolveCallbackFunc,
                                      PhysicsWorld::collisionPostSolveCallbackFunc,
                                      PhysicsWorld::collisionSeparateCallbackFunc,
                                      this);
    
    return true;
}

void PhysicsWorld::addJoint(PhysicsJoint* joint)
{
    auto it = std::find(_joints.begin(), _joints.end(), joint);
    
    if (it == _joints.end())
    {
        _joints.push_back(joint);
        
        for (auto subjoint : joint->_info->joints)
        {
            if (!cpSpaceContainsConstraint(_info->space, subjoint))
            {
                cpSpaceAddConstraint(_info->space, subjoint);
            }
        }
    }
    
}

void PhysicsWorld::removeJoint(PhysicsJoint* joint)
{
    
}

void PhysicsWorld::removeAllJoints()
{
    
}

void PhysicsWorld::addShape(PhysicsShape* shape)
{
    for (auto cps : shape->_info->shapes)
    {
        if (cpSpaceContainsShape(_info->space, cps))
        {
            continue;
        }
        
        if (cpBodyIsStatic(shape->getBody()->_info->body))
        {
            cpSpaceAddStaticShape(_info->space, cps);
        }else
        {
            cpSpaceAddShape(_info->space, cps);
        }
    }
}

void PhysicsWorld::addBody(PhysicsBody* body)
{
    CCASSERT(body != nullptr, "the body can not be nullptr");
    
    if (body->isEnable())
    {
        //is gravity enable
        if (!body->isGravityEnable())
        {
            body->applyForce(-_gravity);
        }
        
        // add body to space
        if (body->isDynamic())
        {
            cpSpaceAddBody(_info->space, body->_info->body);
        }
        
        // add shapes to space
        for (auto shape : body->getShapes())
        {
            addShape(shape);
        }
    }
    
    if (_bodys == nullptr)
    {
        _bodys = Array::create(body, NULL);
        _bodys->retain();
    }else
    {
        _bodys->addObject(body);
    }
}

void PhysicsWorld::removeBody(PhysicsBody* body)
{
    CCASSERT(body != nullptr, "the body can not be nullptr");
    
    if (body->getWorld() == this)
    {
        // reset the gravity
        if (!body->isGravityEnable())
        {
            body->applyForce(-_gravity);
        }
    }
    
    // remove shaps
    for (auto shape : body->getShapes())
    {
        for (auto cps : shape->_info->shapes)
        {
            if (cpSpaceContainsShape(_info->space, cps))
            {
                cpSpaceRemoveShape(_info->space, cps);
            }
        }
    }
    
    // remove body
    if (cpSpaceContainsBody(_info->space, body->_info->body))
    {
        cpSpaceRemoveBody(_info->space, body->_info->body);
    }
    
    if (_bodys != nullptr)
    {
        _bodys->removeObject(body);
    }
}

void PhysicsWorld::removeBodyByTag(int tag)
{
    for (Object* obj : *_bodys)
    {
        PhysicsBody* body = dynamic_cast<PhysicsBody*>(obj);
        if (body->getTag() == tag)
        {
            removeBody(body);
            return;
        }
    }
}

void PhysicsWorld::removeShape(PhysicsShape* shape)
{
    for (auto cps : shape->_info->shapes)
    {
        if (cpSpaceContainsShape(_info->space, cps))
        {
            cpSpaceRemoveShape(_info->space, cps);
        }
    }
}

void PhysicsWorld::update(float delta)
{
    for (auto body : *_bodys)
    {
        body->update(delta);
    }
    
    cpSpaceStep(_info->space, delta);
    
    if (_drawNode)
    {
        _drawNode->removeFromParent();
        _drawNode = nullptr;
    }
    
    if (_debugDraw)
    {
        debugDraw();
    }
}

void PhysicsWorld::debugDraw()
{
    if (_debugDraw && _bodys != nullptr)
    {
        _drawNode= DrawNode::create();

        for (Object* obj : *_bodys)
        {
            PhysicsBody* body = dynamic_cast<PhysicsBody*>(obj);
            
            std::vector<PhysicsShape*> shapes = body->getShapes();
            
            for (auto shape : shapes)
            {
                drawWithShape(_drawNode, shape);
            }
        }
        
        if (_scene)
        {
            _scene->addChild(_drawNode);
        }
    }
}

void PhysicsWorld::setScene(Scene *scene)
{
    _scene = scene;
}

void PhysicsWorld::drawWithShape(DrawNode* node, PhysicsShape* shape)
{
    for (auto it = shape->_info->shapes.begin(); it != shape->_info->shapes.end(); ++it)
    {
        cpShape *shape = *it;
        
        switch ((*it)->klass_private->type)
        {
            case CP_CIRCLE_SHAPE:
            {
                float radius = PhysicsHelper::cpfloat2float(cpCircleShapeGetRadius(shape));
                Point centre = PhysicsHelper::cpv2point(cpBodyGetPos(cpShapeGetBody(shape)))
                + PhysicsHelper::cpv2point(cpCircleShapeGetOffset(shape));
                
                Point seg[4] = {};
                seg[0] = Point(centre.x - radius, centre.y - radius);
                seg[1] = Point(centre.x - radius, centre.y + radius);
                seg[2] = Point(centre.x + radius, centre.y + radius);
                seg[3] = Point(centre.x + radius, centre.y - radius);
                node->drawPolygon(seg, 4, Color4F(), 1, Color4F(1, 0, 0, 1));
                break;
            }
            case CP_SEGMENT_SHAPE:
            {
                cpSegmentShape *seg = (cpSegmentShape *)shape;
                node->drawSegment(PhysicsHelper::cpv2point(seg->ta),
                                  PhysicsHelper::cpv2point(seg->tb),
                                  PhysicsHelper::cpfloat2float(seg->r==0 ? 1 : seg->r), Color4F(1, 0, 0, 1));
                break;
            }
            case CP_POLY_SHAPE:
            {
                cpPolyShape* poly = (cpPolyShape*)shape;
                int num = poly->numVerts;
                Point* seg = new Point[num];
                
                PhysicsHelper::cpvs2points(poly->tVerts, seg, num);
                
                node->drawPolygon(seg, num, Color4F(1.0f, 0.0f, 0.0f, 0.3f), 1.0f, Color4F(1.0f, 0.0f, 0.0f, 1.0f));
                
                delete[] seg;
                break;
            }
            default:
                break;
        }
    }
}

int PhysicsWorld::collisionBeginCallback(PhysicsContact& contact)
{
    bool ret = true;
    PhysicsBody* bodyA = contact.getShapeA()->getBody();
    PhysicsBody* bodyB = contact.getShapeB()->getBody();
    
    if ((bodyA->getCategoryBitmask() & bodyB->getContactTestBitmask()) == 0)
    {
        contact.setNotify(false);
    }
    
    if ((bodyA->getCategoryBitmask() & bodyB->getCollisionBitmask()) == 0)
    {
        ret = false;
    }
    
    if (contact.getNotify() && _listener && _listener->onContactBegin)
    {
        // the mask has high priority than _listener->onContactBegin.
        // so if the mask test is false, the two bodies won't have collision. 
        if (ret)
        {
            ret = _listener->onContactBegin(contact);
        }else
        {
            _listener->onContactBegin(contact);
        }
    }
    
    return ret;
}

int PhysicsWorld::collisionPreSolveCallback(PhysicsContact& contact, const PhysicsContactPreSolve& solve)
{
    if (!contact.getNotify())
    {
        return true;
    }
    
    if (_listener && _listener->onContactPreSolve)
    {
        return _listener->onContactPreSolve(contact, solve);
    }
    
    return true;
}

void PhysicsWorld::collisionPostSolveCallback(PhysicsContact& contact, const PhysicsContactPostSolve& solve)
{
    if (!contact.getNotify())
    {
        return;
    }
    
    if (_listener && _listener->onContactPreSolve)
    {
        _listener->onContactPostSolve(contact, solve);
    }
}

void PhysicsWorld::collisionSeparateCallback(PhysicsContact& contact)
{
    if (!contact.getNotify())
    {
        return;
    }
    
    if (_listener && _listener->onContactEnd)
    {
        _listener->onContactEnd(contact);
    }
}

void PhysicsWorld::setGravity(Point gravity)
{
    if (_bodys != nullptr)
    {
        for (auto child : *_bodys)
        {
            PhysicsBody* body = dynamic_cast<PhysicsBody*>(child);
            
            // reset gravity for body
            if (!body->isGravityEnable())
            {
                body->applyForce(-_gravity);
                body->applyForce(gravity);
            }
        }
    }
    
    _gravity = gravity;
    cpSpaceSetGravity(_info->space, PhysicsHelper::point2cpv(gravity));
}

#elif (CC_PHYSICS_ENGINE == CC_PHYSICS_BOX2D)

#endif

PhysicsWorld* PhysicsWorld::create()
{
    PhysicsWorld * world = new PhysicsWorld();
    if(world && world->init())
    {
        return world;
    }
    
    CC_SAFE_DELETE(world);
    return nullptr;
}

PhysicsWorld::PhysicsWorld()
: _gravity(Point(0.0f, -98.0f))
, _speed(1.0f)
, _info(nullptr)
, _listener(nullptr)
, _bodys(nullptr)
, _scene(nullptr)
, _debugDraw(false)
, _drawNode(nullptr)
{
    
}

PhysicsWorld::~PhysicsWorld()
{
    CC_SAFE_DELETE(_info);
    CC_SAFE_RELEASE(_bodys);
}

NS_CC_END

#endif // CC_USE_PHYSICS
