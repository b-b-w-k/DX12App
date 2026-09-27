#pragma once

class Node;

// 组件基类
// 具体组件继承与它
class Component
{
public:
	Component() = default;
	virtual ~Component() = default;

	virtual void OnAttach() {}       // 附加
	virtual void OnDetach() {}       // 解除
	virtual void Update(float dt) {} // 更新

	Node* owner = nullptr;           // 持有者
};

