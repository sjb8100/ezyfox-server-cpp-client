#pragma once

#include <mutex>
#include "../EzyMacro.h"
#include "EzySocketDefine.h"
#include "../entity/EzyValue.h"

EZY_NAMESPACE_START_WITH_ONLY(event)
class EzyEvent;
EZY_NAMESPACE_END_WITH

EZY_NAMESPACE_START_WITH(socket)

typedef entity::EzyValue EzySocketData;

class EzySocketEventQueue {
	std::mutex mMutex;
    std::vector<event::EzyEvent*> mEvents;
public:
	EzySocketEventQueue();
	~EzySocketEventQueue();

    void addEvent(event::EzyEvent* event);
    void popAll(std::vector<event::EzyEvent*> &buffer);
	void clear();
};

EZY_NAMESPACE_END_WITH

