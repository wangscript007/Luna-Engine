#pragma once

#include "pc.h"
#include "ECSComponent.hpp"

class BaseECSSystem {
private:
    std::vector<uint32> ComponentTypes;
    std::vector<uint32> ComponentFlags;

public:
    enum Flags {
        // If entity doesn't has such component -> we don't even care
        Optional = 1, 

        // If entity has this component -> entity is invalid for this system
        Exclude = 2, 

    };

    BaseECSSystem() {};
    BaseECSSystem(const std::vector<uint32>& In): ComponentTypes(In) {};
    virtual void UpdateComponents(float dt, BaseECSComponent** comp) {};
    inline const std::vector<uint32>& GetComponentTypes() const { return ComponentTypes; }
    inline const std::vector<uint32>& GetComponentFlags() const { return ComponentFlags; }

    bool IsValid();

protected:
    void AddComponentType(uint32 Type, uint32 Flag=0) {
        ComponentTypes.push_back(Type);
        ComponentFlags.push_back(Flag);
    };
};

class ECSSystemList {
private:
    std::vector<BaseECSSystem*> Systems;

public:
    inline bool AddSystem(BaseECSSystem& Sys) {
        if( !Sys.IsValid() ) return false;
        Systems.push_back(&Sys);
        return true;
    }

    bool RemoveSystem(BaseECSSystem* Sys);
    bool HasSystem(BaseECSSystem* Sys) const;
    void Clear();

    inline size_t size() const { return Systems.size(); }
    inline BaseECSSystem* operator[](uint32 index) const { return Systems[index]; }
};
