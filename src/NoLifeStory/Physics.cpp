////////////////////////////////////////////////////
// This file is part of NoLifeStory.              //
// Please see SuperGlobal.h for more information. //
////////////////////////////////////////////////////
#include "Global.h"

const double walkForce = 140000;
const double walkSpeed = 125;
const double walkDrag = 80000;
const double slipForce = 60000;
const double slipSpeed = 120;
const double floatDrag1 = 100000;
const double floatDrag2 = 10000;
const double floatCoefficient = 0.01;
const double swimForce = 120000;
const double swimSpeed = 140;
const double flyForce = 120000;
const double flySpeed = 200;
const double gravityAcc = 2000;
const double fallSpeed = 670;
const double jumpSpeed = 555;
const double maxFriction = 2;
const double minFriction = 0.05;
const double swimSpeedDec = 0.9;
const double flyJumpDec = 0.35;

void NLS::Physics::Init() {
	Node n = WZ["Map"]["Physics"];
	if (n) {
		C("WZ") << "Oh boy, there's physics info!" << endl;
	} else {

	}
}

NLS::Physics::Physics() {
	Reset(0, 0);
}

NLS::Physics::Physics(double x, double y) {
	Reset(x, y);
}

void NLS::Physics::Reset(double x, double y) {
	this->x = x;
	this->y = y;
	vx = 0;
	vy = 0;
	fh = nullptr;
	lr = nullptr;
	layer = 0;
	group = 0;
	freefall = 0;
}

void NLS::Physics::Update() {
	if (vy < fallSpeed) {
		freefall = 0;
	} else {
		freefall += Time.delta;
	}
	//TODO - Move this control stuff elsewhere
	bool left = sf::Keyboard::IsKeyPressed(sf::Keyboard::Left);
	bool right = sf::Keyboard::IsKeyPressed(sf::Keyboard::Right);
	bool up = sf::Keyboard::IsKeyPressed(sf::Keyboard::Up);
	bool down = sf::Keyboard::IsKeyPressed(sf::Keyboard::Down);
	bool jump = sf::Keyboard::IsKeyPressed(sf::Keyboard::LAlt) or sf::Keyboard::IsKeyPressed(sf::Keyboard::RAlt);
	//Temporary flying code
	if (left) {
		x -= Time.delta/2;
	}
	if (right) {
		x += Time.delta/2;
	}
	if (up) {
		y -= Time.delta/2;
	}
	if (down) {
		y += Time.delta/2;
	}
	//TODO - Handle jumping here
	if (fh) {//Walking on the ground

	} else {//Just air
		if (true) {//No water/flying

		} else {//fly!

		}
	}
	//TODO - Grab on to ladders
}