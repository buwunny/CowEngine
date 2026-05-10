#include "PhysicsWorld.hpp"

PhysicsWorld::PhysicsWorld()
{
    collisionConfiguration = new btDefaultCollisionConfiguration();
    dispatcher = new btCollisionDispatcher(collisionConfiguration);
    overlappingPairCache = new btDbvtBroadphase();
    solver = new btSequentialImpulseConstraintSolver();
    dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, overlappingPairCache, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81 * 2, 0));
}

PhysicsWorld::~PhysicsWorld()
{
    delete dynamicsWorld;
    delete solver;
    delete overlappingPairCache;
    delete dispatcher;
    delete collisionConfiguration;
}

void PhysicsWorld::addRigidBody(btRigidBody *body, short group, short mask)
{
    if (!dynamicsWorld || !body)
        return;
    dynamicsWorld->addRigidBody(body, group, mask);
}

void PhysicsWorld::removeRigidBody(btRigidBody *body)
{
    if (!dynamicsWorld || !body)
        return;
    dynamicsWorld->removeRigidBody(body);
}

void PhysicsWorld::rayTest(const btVector3 &from, const btVector3 &to, btCollisionWorld::RayResultCallback &result)
{
    if (!dynamicsWorld)
        return;
    dynamicsWorld->rayTest(from, to, result);
}
