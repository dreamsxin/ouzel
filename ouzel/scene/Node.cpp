// Copyright (C) 2016 Elviss Strazdins
// This file is part of the Ouzel engine.

#include <algorithm>
#include "Node.h"
#include "core/Engine.h"
#include "SceneManager.h"
#include "Layer.h"
#include "animators/Animator.h"
#include "Camera.h"
#include "utils/Utils.h"
#include "math/MathUtils.h"
#include "Component.h"

namespace ouzel
{
    namespace scene
    {
        Node::Node()
        {
            animationUpdateCallback.callback = std::bind(&Node::updateAnimation, this, std::placeholders::_1);
        }

        Node::~Node()
        {
            if (currentAnimator) currentAnimator->parentNode = nullptr;

            for (Component* component : components)
            {
                component->node = nullptr;
            }

            for (Node* child : children)
            {
                child->parent = nullptr;
            }

            if (parent) parent->removeChild(this);
        }

        void Node::visit(std::vector<Node*>& drawQueue,
                         const Matrix4& newParentTransform,
                         bool parentTransformDirty,
                         Camera* camera,
                         int32_t parentOrder)
        {
            worldOrder = parentOrder + order;

            if (parentTransformDirty)
            {
                updateTransform(newParentTransform);
            }

            if (transformDirty)
            {
                calculateTransform();
            }

            if (!hidden)
            {
                AABB2 boundingBox = getBoundingBox();

                if (cullDisabled || (!boundingBox.isEmpty() && camera->checkVisibility(getTransform(), boundingBox)))
                {
                    auto upperBound = std::upper_bound(drawQueue.begin(), drawQueue.end(), this,
                                                       [](Node* a, Node* b) {
                                                           return a->worldOrder > b->worldOrder;
                                                       });

                    drawQueue.insert(upperBound, this);
                }
            }

            for (Node* child : children)
            {
                child->visit(drawQueue, transform, updateChildrenTransform, camera, worldOrder);
            }

            updateChildrenTransform = false;
        }

        void Node::draw(Camera* camera)
        {
            if (transformDirty)
            {
                calculateTransform();
            }

            graphics::Color drawColor(color.r, color.g, color.b, static_cast<uint8_t>(color.a * opacity));

            for (Component* component : components)
            {
                if (!component->isHidden())
                {
                    component->draw(transform, drawColor, camera);
                }
            }
        }

        void Node::drawWireframe(Camera* camera)
        {
            if (transformDirty)
            {
                calculateTransform();
            }

            graphics::Color drawColor(color.r, color.g, color.b, 255);

            for (Component* component : components)
            {
                if (!component->isHidden())
                {
                    component->drawWireframe(transform, drawColor, camera);
                }
            }
        }

        void Node::addChild(Node* node)
        {
            NodeContainer::addChild(node);

            node->updateTransform(getTransform());
        }

        void Node::removeFromParent()
        {
            if (parent)
            {
                parent->removeChild(this);
            }
        }

        void Node::setPosition(const Vector2& newPosition)
        {
            position.x = newPosition.x;
            position.y = newPosition.y;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setPosition(const Vector3& newPosition)
        {
            position = newPosition;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setRotation(float newRotation)
        {
            rotation = newRotation;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setScale(const Vector2& newScale)
        {
            scale = newScale;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setColor(const graphics::Color& newColor)
        {
            color = newColor;
        }

        void Node::setOpacity(float newOpacity)
        {
            opacity = clamp(newOpacity, 0.0f, 1.0f);
        }

        void Node::setFlipX(bool newFlipX)
        {
            flipX = newFlipX;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setFlipY(bool newFlipY)
        {
            flipY = newFlipY;

            localTransformDirty = transformDirty = inverseTransformDirty = true;
        }

        void Node::setHidden(bool newHidden)
        {
            hidden = newHidden;
        }

        bool Node::pointOn(const Vector2& worldPosition) const
        {
            Vector2 localPosition = convertWorldToLocal(worldPosition);

            for (Component* component : components)
            {
                if (component->pointOn(localPosition))
                {
                    return true;
                }
            }

            return false;
        }

        bool Node::shapeOverlaps(const std::vector<Vector2>& edges) const
        {
            Matrix4 inverse = getInverseTransform();

            std::vector<Vector2> transformedEdges;

            for (const Vector2& edge : edges)
            {
                Vector3 transformedEdge = edge;

                inverse.transformPoint(transformedEdge);

                transformedEdges.push_back(Vector2(transformedEdge.x, transformedEdge.y));
            }

            for (Component* component : components)
            {
                if (component->shapeOverlaps(transformedEdges))
                {
                    return true;
                }
            }

            return false;
        }

        void Node::updateTransform(const Matrix4& newParentTransform)
        {
            parentTransform = newParentTransform;
            transformDirty = inverseTransformDirty = true;
        }

        Vector3 Node::getWorldPosition() const
        {
            Vector3 result = position;
            getTransform().transformPoint(result);

            return position;
        }

        Vector2 Node::convertWorldToLocal(const Vector2& worldPosition) const
        {
            Vector3 localPosition = worldPosition;

            const Matrix4& currentInverseTransform = getInverseTransform();
            currentInverseTransform.transformPoint(localPosition);

            return Vector2(localPosition.x, localPosition.y);
        }

        Vector2 Node::convertLocalToWorld(const Vector2& localPosition) const
        {
            Vector3 worldPosition = localPosition;

            const Matrix4& currentTransform = getTransform();
            currentTransform.transformPoint(worldPosition);

            return Vector2(worldPosition.x, worldPosition.y);
        }

        void Node::animate(Animator* animator)
        {
            if (currentAnimator)
            {
                currentAnimator->parentNode = nullptr;
                currentAnimator->stop();
            }

            currentAnimator = animator;

            if (currentAnimator)
            {
                currentAnimator->removeFromParent();
                currentAnimator->parentNode = this;
                currentAnimator->start(this);
            }

            sharedEngine->scheduleUpdate(&animationUpdateCallback);
        }

        void Node::removeAnimator(Animator* animator)
        {
            if (animator && animator == currentAnimator)
            {
                currentAnimator->parentNode = nullptr;
                currentAnimator->stop();
                currentAnimator = nullptr;
                sharedEngine->unscheduleUpdate(&animationUpdateCallback);
            }
        }

        void Node::removeCurrentAnimator()
        {
            if (currentAnimator)
            {
                currentAnimator->parentNode = nullptr;
                currentAnimator->stop();
                currentAnimator = nullptr;
                sharedEngine->unscheduleUpdate(&animationUpdateCallback);
            }
        }

        void Node::calculateLocalTransform() const
        {
            localTransform = Matrix4::IDENTITY;
            localTransform.translate(position);
            if (rotation != 0.0f) localTransform.rotateZ(-rotation);

            Vector3 realScale = Vector3(scale.x * (flipX ? -1.0f : 1.0f),
                                        scale.y * (flipY ? -1.0f : 1.0f),
                                        1.0f);

            localTransform.scale(realScale);

            localTransformDirty = false;
        }

        void Node::calculateTransform() const
        {
            transform = parentTransform * getLocalTransform();
            transformDirty = false;

            updateChildrenTransform = true;
        }

        void Node::calculateInverseTransform() const
        {
            if (transformDirty)
            {
                calculateTransform();
            }

            inverseTransform = transform;
            inverseTransform.invert();
            inverseTransformDirty = false;
        }

        void Node::addComponent(Component* component)
        {
            Node* oldNode = component->node;

            if (oldNode)
            {
                oldNode->removeComponent(component);
            }

            component->node = this;
            components.push_back(component);
        }

        bool Node::removeComponent(uint32_t index)
        {
            if (index >= components.size())
            {
                return false;
            }

            Component* component = components[index];
            component->node = nullptr;

            components.erase(components.begin() + static_cast<int>(index));

            return true;
        }

        bool Node::removeComponent(Component* component)
        {
            for (auto i = components.begin(); i != components.end();)
            {
                if (*i == component)
                {
                    component->node = nullptr;
                    components.erase(i);
                    return true;
                }
                else
                {
                    ++i;
                }
            }

            return false;
        }

        void Node::removeAllComponents()
        {
            components.clear();
        }

        void Node::updateAnimation(float delta)
        {
            if (currentAnimator)
            {
                currentAnimator->update(delta);

                if (currentAnimator->isDone())
                {
                    removeCurrentAnimator();
                    sharedEngine->unscheduleUpdate(&animationUpdateCallback);
                }
            }
            else
            {
                sharedEngine->unscheduleUpdate(&animationUpdateCallback);
            }
        }

        AABB2 Node::getBoundingBox() const
        {
            AABB2 boundingBox;

            for (Component* component : components)
            {
                if (!component->isHidden())
                {
                    boundingBox.merge(component->getBoundingBox());
                }
            }

            return boundingBox;
        }
    } // namespace scene
} // namespace ouzel
