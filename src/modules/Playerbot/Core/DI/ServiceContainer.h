/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "Define.h"
#include "Threading/LockHierarchy.h"
#include <memory>
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <stdexcept>

namespace Playerbot
{

/**
 * @brief Dependency Injection Container for Playerbot services
 *
 * This container implements the Service Locator pattern with dependency injection,
 * replacing the existing Meyer's Singleton pattern throughout the Playerbot codebase.
 *
 * **Benefits:**
 * - Testability: Services can be mocked for unit testing
 * - Visibility: Dependencies are explicit in constructors
 * - Flexibility: Service implementations can be swapped at runtime
 * - Thread-safety: Uses OrderedMutex for deadlock prevention
 *
 * **Usage:**
 * @code
 * // 1. Register service (at startup)
 * Services::Container().RegisterSingleton<ISpatialGridManager, SpatialGridManager>();
 *
 * // 2. Resolve service (in code)
 * auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
 *
 * // 3. Use service
 * auto grid = spatialMgr->GetGrid(mapId);
 * @endcode
 *
 * **Design Pattern:**
 * - Service Locator for transitional compatibility with existing code
 * - Constructor Injection for new code (preferred)
 * - Lazy initialization with factory functions
 * - Thread-safe singleton lifecycle management
 *
 * **Thread Safety:**
 * - All operations are thread-safe
 * - Uses OrderedMutex<CONFIG_MANAGER> (Layer 1) to prevent deadlocks
 * - Service factories are called once and cached
 *
 * @note This is a transitional pattern. New code should prefer constructor injection
 *       over service location. The container is primarily for compatibility with
 *       existing singleton-heavy code during migration.
 */
class TC_GAME_API ServiceContainer
{
public:
    ServiceContainer() = default;
    ~ServiceContainer() = default;

    // Non-copyable, non-movable
    ServiceContainer(ServiceContainer const&) = delete;
    ServiceContainer& operator=(ServiceContainer const&) = delete;
    ServiceContainer(ServiceContainer&&) = delete;
    ServiceContainer& operator=(ServiceContainer&&) = delete;

    /**
     * @brief Register a singleton service with implementation type
     *
     * Creates the service immediately using default constructor.
     *
     * @tparam TInterface Interface type (abstract base class)
     * @tparam TImpl Implementation type (concrete class)
     *
     * @throws std::runtime_error if service is already registered
     *
     * Example:
     * @code
     * container.RegisterSingleton<ISpatialGridManager, SpatialGridManager>();
     * @endcode
     */
    template<typename TInterface, typename TImpl>
    void RegisterSingleton()
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);

        if (_services.find(typeId) != _services.end())
        {
            throw ::std::runtime_error(
                ::std::string("Service already registered: ") + typeId.name());
        }

        // Create service instance immediately
        _services[typeId] = ::std::make_shared<TImpl>();
    }

    /**
     * @brief Register a singleton service with existing instance
     *
     * Useful for pre-constructed instances or instances with dependencies.
     *
     * @tparam TInterface Interface type
     * @param instance Shared pointer to service instance
     *
     * @throws std::runtime_error if service is already registered
     *
     * Example:
     * @code
     * auto spatialMgr = std::make_shared<SpatialGridManager>(config);
     * container.RegisterInstance<ISpatialGridManager>(spatialMgr);
     * @endcode
     */
    template<typename TInterface>
    void RegisterInstance(::std::shared_ptr<TInterface> instance)
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);

        if (_services.find(typeId) != _services.end())
        {
            throw ::std::runtime_error(
                ::std::string("Service already registered: ") + typeId.name());
        }

        _services[typeId] = instance;
    }

    /**
     * @brief Register a service with custom factory function
     *
     * Factory is called lazily on first Resolve() call.
     * Useful for services with complex initialization or dependencies.
     *
     * @tparam TInterface Interface type
     * @param factory Function that creates the service instance
     *
     * @throws std::runtime_error if service is already registered
     *
     * Example:
     * @code
     * container.RegisterFactory<ISpatialGridManager>([]() {
     *     return std::make_shared<SpatialGridManager>(config, dependencies);
     * });
     * @endcode
     */
    template<typename TInterface>
    void RegisterFactory(::std::function<::std::shared_ptr<TInterface>()> factory)
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);

        if (_services.find(typeId) != _services.end() ||
            _factories.find(typeId) != _factories.end())
        {
            throw ::std::runtime_error(
                ::std::string("Service already registered: ") + typeId.name());
        }

        // Store factory as type-erased function
        _factories[typeId] = [factory]() -> ::std::shared_ptr<void> {
            return factory();
        };
    }

    /**
     * @brief Resolve (retrieve) a service instance
     *
     * If service was registered with factory, creates it on first call.
     * Subsequent calls return the cached instance.
     *
     * @tparam TInterface Interface type to resolve
     * @return Shared pointer to service, or nullptr if not registered
     *
     * @note Thread-safe. Factory is called at most once.
     *
     * Example:
     * @code
     * auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
     * if (spatialMgr)
     * {
     *     auto grid = spatialMgr->GetGrid(mapId);
     * }
     * @endcode
     */
    template<typename TInterface>
    ::std::shared_ptr<TInterface> Resolve()
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);

        // Check if already instantiated
        auto it = _services.find(typeId);
        if (it != _services.end())
        {
            return ::std::static_pointer_cast<TInterface>(it->second);
        }

        // Try to create from factory
        auto factoryIt = _factories.find(typeId);
        if (factoryIt != _factories.end())
        {
            // Call factory and cache result
            auto service = factoryIt->second();
            _services[typeId] = service;
            _factories.erase(factoryIt);  // Factory no longer needed

            return ::std::static_pointer_cast<TInterface>(service);
        }

        // Service not registered
        return nullptr;
    }

    /**
     * @brief Resolve service with exception if not found
     *
     * Same as Resolve() but throws exception instead of returning nullptr.
     * Useful when service is required and missing service is a bug.
     *
     * @tparam TInterface Interface type to resolve
     * @return Shared pointer to service
     * @throws std::runtime_error if service is not registered
     *
     * Example:
     * @code
     * auto spatialMgr = Services::Container().RequireService<ISpatialGridManager>();
     * // No null check needed - exception thrown if missing
     * auto grid = spatialMgr->GetGrid(mapId);
     * @endcode
     */
    template<typename TInterface>
    ::std::shared_ptr<TInterface> RequireService()
    {
        auto service = Resolve<TInterface>();
        if (!service)
        {
            throw ::std::runtime_error(
                ::std::string("Required service not registered: ") + typeid(TInterface).name());
        }
        return service;
    }

    /**
     * @brief Check if service is registered
     *
     * @tparam TInterface Interface type to check
     * @return True if service is registered (instance or factory), false otherwise
     *
     * Example:
     * @code
     * if (Services::Container().IsRegistered<ISpatialGridManager>())
     * {
     *     auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
     * }
     * @endcode
     */
    template<typename TInterface>
    bool IsRegistered() const
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);
        return _services.find(typeId) != _services.end() ||
               _factories.find(typeId) != _factories.end();
    }

    /**
     * @brief Unregister a service
     *
     * Removes service from container. Useful for testing or shutdown.
     *
     * @tparam TInterface Interface type to unregister
     * @return True if service was unregistered, false if not registered
     *
     * @note Any existing shared_ptr references to the service remain valid
     *
     * Example:
     * @code
     * Services::Container().Unregister<ISpatialGridManager>();
     * @endcode
     */
    template<typename TInterface>
    bool Unregister()
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        auto typeId = typeid(TInterface);

        bool removed = false;
        removed |= _services.erase(typeId) > 0;
        removed |= _factories.erase(typeId) > 0;

        return removed;
    }

    /**
     * @brief Clear all registered services
     *
     * Useful for shutdown or test cleanup.
     *
     * @note Any existing shared_ptr references to services remain valid
     */
    void Clear()
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        _services.clear();
        _factories.clear();
    }

    /**
     * @brief Get number of registered services
     *
     * Includes both instantiated services and pending factories.
     *
     * @return Count of registered services
     */
    size_t GetServiceCount() const
    {
        ::std::lock_guard<decltype(_mutex)> lock(_mutex);

        return _services.size() + _factories.size();
    }

private:
    // Service instances (instantiated services)
    ::std::unordered_map<::std::type_index, ::std::shared_ptr<void>> _services;

    // Service factories (lazy services)
    ::std::unordered_map<::std::type_index, ::std::function<::std::shared_ptr<void>()>> _factories;

    // Thread safety (Layer 1 - acquired before all other locks)
    mutable OrderedRecursiveMutex<LockOrder::CONFIG_MANAGER> _mutex;
};

/**
 * @brief Global service locator for transitional compatibility
 *
 * Provides global access to the DI container.
 * This is a transitional pattern - prefer constructor injection for new code.
 *
 * **Usage:**
 * @code
 * // Register services at startup
 * Services::Container().RegisterSingleton<ISpatialGridManager, SpatialGridManager>();
 *
 * // Resolve services anywhere
 * auto spatialMgr = Services::Container().Resolve<ISpatialGridManager>();
 * @endcode
 *
 * **Migration Strategy:**
 * 1. Replace Meyer's Singletons with Services::Container().Resolve<T>()
 * 2. Add interfaces for testability
 * 3. Convert classes to constructor injection
 * 4. Eventually remove Services global accessor in favor of pure DI
 */
class TC_GAME_API Services
{
public:
    /**
     * @brief Get global service container instance
     *
     * @return Reference to singleton ServiceContainer
     *
     * Thread-safe: Uses Meyer's Singleton (C++11 guarantee)
     */
    static ServiceContainer& Container()
    {
        static ServiceContainer instance;
        return instance;
    }

private:
    Services() = delete;
};

} // namespace Playerbot
