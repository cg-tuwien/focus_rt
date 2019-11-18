#include "includes.h"
#define _USE_MATH_DEFINES
#include <Math.h>

flevel3logic::flevel3logic(fscene* scene) : flevellogic(scene) {
}

void flevel3logic::initialize() {
	initialCameraPos = scene->get_camera().translation();
	initialCameraRot = scene->get_camera().rotation();

	//---- CREATE PHYSICS AND PLAYER OBJECTS -----
	physics = std::make_unique<fphysicscontroller>(scene);
	player = std::make_unique<fplayercontrol>(physics.get(), scene, false, 1.5, (PxUserControllerHitReport*)this);

	//---- CREATE RIGID BODIES ----
	std::vector<std::string> floornames = {
		"Floor1", "Floor2", "FinalFloor"
	};

	//Floors
	for (uint32_t i = 0; i < floornames.size(); ++i) {
		auto instance = scene->get_model_by_name(floornames[i]);
		auto actor = physics->create_rigid_static_for_scaled_unit_box(instance, (i == 2));
		if (i == 2) {
			movingFloorActor = actor;
			player->set_final_region(movingFloorActor);
		}
	}

	//Wall
	auto wallX = scene->get_model_by_name("WallX");
	physics->create_rigid_static_for_scaled_unit_box(wallX);

	//DoorWalls
	std::vector<std::string> doorwalls = {
		"DoorP1", "DoorP2"
	};
	for (const std::string& name : doorwalls) {
		auto instance = scene->get_model_by_name(name);
		physics->create_rigid_static_for_scaled_plane(instance);
	}
	//"Invisible" Walls
	PxShape* wallShape = physics->mPhysics->createShape(PxBoxGeometry(2.93, 3.23, 0.1), *physics->mDefaultMaterial, false);
	PxTransform wall1t = PxTransform(PxVec3(-5.44, 2.93, 5.55), PxQuat(M_PI / 2, PxVec3(0, 0, 1)));
	PxRigidStatic* wall1a = physics->mPhysics->createRigidStatic(wall1t);
	wall1a->attachShape(*wallShape);
	physics->mPxScene->addActor(*wall1a);
	PxTransform wall2t = PxTransform(PxVec3(5.44, 2.93, 5.55), PxQuat(M_PI / 2, PxVec3(0, 0, 1)));
	PxRigidStatic* wall2a = physics->mPhysics->createRigidStatic(wall2t);
	wall2a->attachShape(*wallShape);
	physics->mPxScene->addActor(*wall2a);
	PxTransform wall3t = PxTransform(PxVec3(5.44, 2.93, 3.37), PxQuat(M_PI / 2, PxVec3(0, 0, 1)));
	PxRigidStatic* wall3a = physics->mPhysics->createRigidStatic(wall3t);
	wall3a->attachShape(*wallShape);
	physics->mPxScene->addActor(*wall3a);
	PxTransform wall4t = PxTransform(PxVec3(-5.44, 2.93, 3.37), PxQuat(M_PI / 2, PxVec3(0, 0, 1)));
	PxRigidStatic* wall4a = physics->mPhysics->createRigidStatic(wall4t);
	wall4a->attachShape(*wallShape);
	physics->mPxScene->addActor(*wall4a);

	wallShape->release();


	//Mirror
	auto mirrorBorderInstance = scene->get_model_by_name("MirrorBorder1");
	auto mirrorPlaneInstance = scene->get_model_by_name("MirrorPlane1");
	mirrorBorderActor = physics->create_rigid_static_for_scaled_unit_box(mirrorBorderInstance, true);
	mirrorPlaneActor = physics->create_rigid_static_for_scaled_plane(mirrorPlaneInstance, true);
	player->add_mirror({ mirrorBorderActor, mirrorPlaneActor }, 14.75f);

	mirrorPlaneOriginalTransform = mirrorPlaneActor->getGlobalPose();
	mirrorBorderOriginalTransform = mirrorBorderActor->getGlobalPose();

	//Rotating Wall
	auto movingWallInstance = scene->get_model_by_name("RotWall");

	movingWallActor = physics->create_rigid_static_for_scaled_unit_box(movingWallInstance, true);
	movingWallPxOriginalTransformation = movingWallActor->getGlobalPose();

	//Sphere
	sphereInstance = scene->get_model_by_name("Sphere");

	interpolator.add_sample(0, glm::vec3(60, 0, 0.7));
	interpolator.add_sample(1, glm::vec3(60, 1, 1));
}

levelstatus flevel3logic::update(float deltaT, double focusHitValue)
{
	if (score > 2.0f) {
		return levelstatus::WON;
	}
	if (std::fabs(focusHitValue) < 0.0001) {
		score = glm::max(score - 0.05 * deltaT, 0.0);
	}
	else {
		score = glm::min(score + deltaT * 0.1, 1.0);
	}

	scene->set_background_color(interpolator.interpolate(score));
	if (score >= 0.99 && player->on_final_region()) {
		scene->get_material_data(sphereInstance->mMaterialIndex).mDiffuseReflectivity = glm::vec4(1.0f);
		scene->set_background_color(glm::vec4(1, 1, 0.5, 1));
		score = 100.0f;
		return levelstatus::WON;
	}

	if (cgb::input().key_released(cgb::key_code::f10)) {
		score = 100.0f;
		return levelstatus::WON;
	}

	if (player->fell_down()) {
		return levelstatus::LOST;
	}
	return levelstatus::RUNNING;
}

void flevel3logic::fixed_update(float stepSize)
{
	//Move Moving Floor
	float tsin = sin(platformAccTime / 5.0);
	float tcos = cos(platformAccTime / 5.0);
	float floorx = -(tsin * 12.0f);
	float floorz = -18.0f + tcos * 12.0f;
	auto oldfloorpose = movingFloorActor->getGlobalPose();
	auto newfloorpose = PxTransform(PxVec3(floorx, oldfloorpose.p.y, floorz), oldfloorpose.q);
	movingFloorActor->setGlobalPose(newfloorpose);

	//Move Rotating Wall
	float rotwallangle = 0.0f;
	if (tcos * tsin < 0.0) {
		if (tcos < 0.0) {
			rotwallangle = M_PI / 2.0 * (1.0 - pow(tsin, 2));
		}
		else {
			rotwallangle = M_PI / 2.0 * pow(tsin, 2);
		}
	}
	else if (tsin < 0.0) {
		rotwallangle = M_PI / 2.0;
	}
	PxQuat rot = PxQuat(rotwallangle, PxVec3(0, 1, 0));
	PxQuat newPxQuat = rot * movingWallPxOriginalTransformation.q;
	movingWallActor->setGlobalPose(PxTransform(movingWallPxOriginalTransformation.p, newPxQuat));

	//Perform Physics and Controller Update
	player->pre_px_update(stepSize);
	physics->update(stepSize);
	player->post_px_update(stepSize);
	double updateTime = glfwGetTime();


	//Check if on Platform
	if (platformMoving) {
		platformAccTime += stepSize;
	}
	if (onPlatform) {
		PxVec3 diffvec = newfloorpose.p - oldfloorpose.p;
		glm::vec3 distance = glm::make_vec3(&diffvec.x);
		player->beam_along(distance);
		onPlatform = false;
	}
}

void flevel3logic::onShapeHit(const PxControllerShapeHit& hit)
{
	if (hit.actor == movingFloorActor) {
		platformMoving = true;
		onPlatform = true;
	}
}

void flevel3logic::reset()
{
	platformAccTime = 0;
	onPlatform = false;
	platformMoving = false;
	scene->get_camera().set_rotation(initialCameraRot);
	scene->get_camera().set_translation(initialCameraPos);
	mirrorBorderActor->setGlobalPose(mirrorBorderOriginalTransform);
	mirrorPlaneActor->setGlobalPose(mirrorPlaneOriginalTransform);
	player->update_position();
	player->reset_mirrors();
	score = 0;
}