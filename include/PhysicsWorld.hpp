#ifndef PHYSICSWORLD_HPP
#define PHYSICSWORLD_HPP

#include <bullet/btBulletDynamicsCommon.h>

class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    btDiscreteDynamicsWorld *getWorld() { return dynamicsWorld; }

    // Convenience wrappers
    void addRigidBody(btRigidBody *body, short group = btBroadphaseProxy::DefaultFilter, short mask = btBroadphaseProxy::AllFilter);
    void removeRigidBody(btRigidBody *body);
    void rayTest(const btVector3 &from, const btVector3 &to, btCollisionWorld::RayResultCallback &result);
    void setGravity(const btVector3 &g)
    {
        if (dynamicsWorld)
            dynamicsWorld->setGravity(g);
    }
    btVector3 getGravity() const { return dynamicsWorld ? dynamicsWorld->getGravity() : btVector3(0, 0, 0); }
    void stepSimulation(float deltaTime, int maxSubSteps = 10)
    {
        if (dynamicsWorld)
            dynamicsWorld->stepSimulation(deltaTime, maxSubSteps);
    }

private:
    btDefaultCollisionConfiguration *collisionConfiguration;
    btCollisionDispatcher *dispatcher;
    btBroadphaseInterface *overlappingPairCache;
    btSequentialImpulseConstraintSolver *solver;
    btDiscreteDynamicsWorld *dynamicsWorld;
};

#endif // PHYSICSWORLD_HPP
