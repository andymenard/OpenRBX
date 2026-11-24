#include "tool/ToolsSurface.h"

#include "appdraw/DrawAdorn.h"
#include "humanoid/Humanoid.h"
#include "rbxgraphics/Adorn.h"
#include "tool/DragTool.h"
#include "tool/DragUtilities.h"
#include "tool/Dragger.h"
#include "tool/GrabTool.h"
#include "tool/MegaDragger.h"
#include "tool/PartDragTool.h"
#include "util/ContentProvider.h"
#include "util/FrameInterpolator.h"
#include "util/Math.h"
#include "util/Sound.h"
#include "util/SoundWorld.h"
#include "util/Units.h"
#include "util/standardout.h"
#include "v8datamodel/Commands.h"
#include "v8datamodel/DataModel.h"
#include "v8datamodel/Explosion.h"
#include "v8datamodel/ModelInstance.h"
#include "v8datamodel/PartInstance.h"
#include "v8datamodel/Surface.h"
#include "v8datamodel/Workspace.h"
#include "v8kernel/Constants.h"
#include "v8tree/Service.h"
#include "v8xml/SerializerV2.h"

#include <G3D/CoordinateFrame.h>
#include <G3D/LineSegment.h>

namespace RBX {

char sFlatTool[] = "Flat";

char sGlueTool[] = "Glue";

char sWeldTool[] = "Weld";

char sStudsTool[] = "Studs";

char sInletTool[] = "Inlet";

char sHingeTool[] = "Hinge";

char sRightMotorTool[] = "RightMotor";

char sLeftMotorTool[] = "LeftMotor";

char sOscillateMotorTool[] = "OscillateMotor";

char sModelSetFrontTool[] = "ModelSetFront";

char sAnchorTool[] = "Anchor";

char sLockTool[] = "Lock";

char sSlingshotTool[] = "Slingshot";

char sRocketTool[] = "Rocket";

char sResizeTool[] = "Resize";

char sHammerTool[] = "Hammer";

char sLaserTool[] = "Laser";

char sGrabTool[] = "Grab";

char sCloneTool[] = "Clone";

char sGameTool[] = "GameTool";

char sAxisMoveTool[] = "AxisMove";

void FlatTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->flat();
	surface->getPartInstance()->join();
}

void HingeTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->setSurfaceType(ROTATE);
	surface->setSurfaceInput(Controller::NO_INPUT);
	surface->setParamA(0.0f);
	surface->setParamB(0.0f);
	surface->getPartInstance()->join();
}

void RightMotorTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->setSurfaceType(ROTATE_V);
	surface->setSurfaceInput(Controller::RIGHT_TRACK_INPUT);
	surface->setParamA(-0.1f);
	surface->setParamB(0.1f);
	surface->getPartInstance()->join();
}

void LeftMotorTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->setSurfaceType(ROTATE_V);
	surface->setSurfaceInput(Controller::LEFT_TRACK_INPUT);
	surface->setParamA(-0.1f);
	surface->setParamB(0.1f);
	surface->getPartInstance()->join();
}

void OscillateMotorTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->setSurfaceType(ROTATE_V);
	surface->setSurfaceInput(Controller::SIN_INPUT);
	surface->setParamA(-0.1f);
	surface->setParamB(0.1f);
	surface->getPartInstance()->join();
}

void SurfaceTool::render3dAdorn(Adorn* adorn)
{
	if (part) {
		DrawAdorn::partSurface(part->getPart(), surfaceId, adorn, G3D::Color4(G3D::Color3::orange(), 1.0f));
	}
}

void ModelSetFrontTool::applyTo(Surface* surface)
{
	PartInstance* part = surface->getPartInstance();
	if (ModelInstance* model = dynamic_cast<ModelInstance*>(part->getTopLevelPVParent())) {
		model->setFront(Math::getWorldNormal(surface->getNormalId(), part->getCoordinateFrame()));
	}
}

SurfaceTool::SurfaceTool(Workspace* workspace) : MouseCommand(workspace), surface(nullptr)
{
}

void SurfaceTool::onMouseHover(const UIEvent& uiEvent)
{
	PartInstance* hitPart;
	surface = getSurface(uiEvent, hitPart, surfaceId);
	part = shared_from(surface ? hitPart : nullptr);
}

MouseCommand* SurfaceTool::onMouseDown(const UIEvent& uiEvent)
{
	onMouseHover(uiEvent);
	if (surface) {
		undoState.reset(new UndoState());
		undoState->addAllProperties(*part);
		applyTo(surface);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);

		std::unique_ptr<RedoState> redoState(new RedoState());
		redoState->addAllProperties(*part);
		workspace->getDataState().pushState(m_undoName, undoState.release(), redoState.release());
		workspace->getDataState().setDirty(true);
	}
	return nullptr;
}

void GlueTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->flat();
	surface->setSurfaceType(GLUE);
	surface->getPartInstance()->join();
	ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
}

void WeldTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->flat();
	surface->setSurfaceType(WELD);
	surface->getPartInstance()->join();
	ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
}

void StudsTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->flat();
	surface->setSurfaceType(STUDS);
	surface->getPartInstance()->join();
	ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
}

void InletTool::applyTo(Surface* surface)
{
	surface->getPartInstance()->destroyJoints();
	surface->flat();
	surface->setSurfaceType(INLET);
	surface->getPartInstance()->join();
	ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
}

void setAnchoredRecursive(Instance* instance, const bool& value);

struct SetAnchored
{
	bool value;

	SetAnchored(bool value) : value(value) {}

	void operator()(Instance* instance) const { setAnchoredRecursive(instance, value); }
};

void setAnchoredRecursive(Instance* instance, const bool& value)
{
	bool anchored = value;
	if (PartInstance* part = dynamic_cast<PartInstance*>(instance)) {
		part->setAnchored(anchored);
	}
	else {
		instance->for_eachChild(SetAnchored(anchored));
	}
}

void ModelTool::render3dAdorn(Adorn* adorn)
{
	if (m_model) {
		if (IRenderable* renderable = dynamic_cast<IRenderable*>(m_model.get())) {
			renderable->render3dSelect(adorn, SELECT_LIMIT);
		}
	}
}

const std::string AnchorTool::getCursorName() const
{
	if (m_model) {
		if (m_anchored) {
			return "UnAnchorCursor";
		}
		else {
			return "AnchorCursor";
		}
	}
	return "AnchorCursor";
}

const std::string LockTool::getCursorName() const
{
	if (m_model) {
		if (PartInstance::getLocked(m_model.get())) {
			return "UnlockCursor";
		}
		else {
			return "LockCursor";
		}
	}
	return "LockCursor";
}

ModelTool::ModelTool(Workspace* workspace) : MouseCommand(workspace)
{
}

static Instance* findUnanchoredPart(Instance* instance)
{
	PartInstance* part = dynamic_cast<PartInstance*>(instance);
	if (part && !part->getAnchored()) {
		return part;
	}

	std::shared_ptr<const std::vector<std::shared_ptr<Instance>>> children = instance->getChildren().read();
	if (children) {
		std::vector<std::shared_ptr<Instance>>::const_iterator end = children->end();
		for (std::vector<std::shared_ptr<Instance>>::const_iterator iter = children->begin(); iter != end; ++iter) {
			if (findUnanchoredPart(iter->get())) {
				return iter->get();
			}
		}
	}
	return nullptr;
}

void ModelTool::onMouseHover(const UIEvent& uiEvent)
{
	PartInstance* part = getPart(uiEvent);
	if (part) {
		m_model = shared_from(Workspace::findTopInstance(part));
	}
}

void AnchorTool::onMouseHover(const UIEvent& uiEvent)
{
	std::shared_ptr<Instance> oldInstance = m_model;
	ModelTool::onMouseHover(uiEvent);
	if (oldInstance != m_model) {
		bool anchored;
		if (m_model) {
			anchored = findUnanchoredPart(m_model.get()) == nullptr;
		}
		else {
			anchored = false;
		}
		m_anchored = anchored;
	}
}

MouseCommand* AnchorTool::onMouseDown(const UIEvent& uiEvent)
{
	if (m_model && !PartInstance::getLocked(m_model.get())) {
		bool anchored = findUnanchoredPart(m_model.get()) != nullptr;
		setAnchoredRecursive(m_model.get(), anchored);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
	}
	return this;
}

MouseCommand* LockTool::onMouseDown(const UIEvent& uiEvent)
{
	if (m_model) {
		bool locked = PartInstance::getLocked(m_model.get());
		PartInstance::setLocked(m_model.get(), !locked);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);
	}
	return this;
}

const std::string ShootTool::getCursorName() const
{
	if (m_reloadCountdown && m_reloadFrames > 10) {
		return "GunWaitCursor";
	}
	return "GunCursor";
}

void ShootTool::onMouseIdle(const UIEvent& uiEvent)
{
	if (m_reloadCountdown > 0) {
		m_reloadCountdown--;
	}
	if (captured()) {
		onMouseMove(uiEvent);
	}
}

PV SlingshotTool::computeLaunch(const UIEvent& uiEvent, const CoordinateFrame& cframe)
{
	G3D::Vector3 hitWorld;
	PartInstance* part = getPart(uiEvent, nullptr, hitWorld);
	G3D::Vector3 direction = part ? (hitWorld - cframe.translation).direction() : cframe.lookVector();
	G3D::Vector3 position = cframe.translation + direction * 3.0f;
	if (!part) {
		CoordinateFrame frame(position);
		return PV(frame, Velocity(direction * 85.0f, G3D::Vector3::zero()));
	}
	else {
		G3D::Vector3 delta = hitWorld - position;
		float height = delta.y;
		delta.y = 0.0f;
		float distance = delta.unitize();
		float angle = Math::computeLaunchAngle(
			85.0f, distance, height, Units::kmsAccelerationToRbx(Constants::getKmsGravity()).y
		);
		delta.y = sin(angle);
		delta.x *= cos(angle);
		delta.z *= cos(angle);
		CoordinateFrame frame(position);
		return PV(frame, Velocity(delta * 85.0f, G3D::Vector3::zero()));
	}
}

PV RocketTool::computeLaunch(const UIEvent& uiEvent, const CoordinateFrame& cframe)
{
	G3D::Vector3 hitWorld;
	PartInstance* part = getPart(uiEvent, nullptr, hitWorld);
	G3D::Vector3 direction = part ? (hitWorld - cframe.translation).direction() : cframe.lookVector();
	CoordinateFrame frame;
	frame.lookAt(direction);
	frame.translation = cframe.translation + 5.0f * direction;
	return PV(frame, Velocity::zero());
}

ShootTool::ShootTool(Workspace* workspace, bool automatic, int reloadFrames)
	: MouseCommand(workspace), m_automatic(automatic), m_reloadFrames(reloadFrames), m_reloadCountdown(0)
{
}

std::shared_ptr<PartInstance> SlingshotTool::createProjectile()
{
	std::vector<std::shared_ptr<Instance>> instances;
	ContentProvider::singleton().load(ContentId::fromAssets("Fonts\\SlingshotPellet.rbxm"), instances);
	return instances.empty() ? nullptr : std::dynamic_pointer_cast<PartInstance>(instances.front());
}

std::shared_ptr<PartInstance> RocketTool::createProjectile()
{
	std::vector<std::shared_ptr<Instance>> instances;
	ContentProvider::singleton().load(ContentId::fromAssets("Fonts\\Rocket.rbxm"), instances);
	return instances.empty() ? nullptr : std::dynamic_pointer_cast<PartInstance>(instances.front());
}

void ShootTool::fire(const UIEvent& uiEvent)
{
	m_reloadCountdown = m_reloadFrames;
	PartInstance* head = Humanoid::getLocalHeadFromContext(workspace);
	if (head) {
		if (!m_projectile) {
			try {
				m_projectile = createProjectile();
			}
			catch (const std::exception& error) {
				StandardOut::singleton()->print(MESSAGE_ERROR, error);
				return;
			}
			if (!m_projectile) {
				StandardOut::singleton()->print(MESSAGE_ERROR, "Projectile asset does not contain a Part");
				return;
			}
		}

		std::shared_ptr<PartInstance> projectile = std::static_pointer_cast<PartInstance>(m_projectile->clone());
		projectile->getPrimitive()->setPV(computeLaunch(uiEvent, head->getCoordinateFrame()));
		// Parenting starts the projectile script, which needs the launch position and direction.
		projectile->setParent(workspace);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(fireSound());
	}
}

MouseCommand* ShootTool::onMouseDown(const UIEvent& uiEvent)
{
	capture();
	if (m_reloadCountdown == 0) {
		fire(uiEvent);
	}
	return this;
}

void ShootTool::onMouseMove(const UIEvent& uiEvent)
{
	if (m_automatic && m_reloadCountdown == 0) {
		fire(uiEvent);
	}
}

const std::string ResizeTool::getCursorName() const
{
	return m_overHandle ? std::string("ResizeCursor") : ArrowToolBase::getCursorName();
}

void ResizeTool::saveState(XmlState* state)
{
	std::shared_ptr<PVInstance> locked = m_handlePart.lock();
	if (PartInstance* part = dynamic_cast<PartInstance*>(locked.get())) {
		part->writeResizeData(state);
	}
}

MouseCommand* ResizeTool::onMouseUp(const UIEvent& uiEvent)
{
	if (captured()) {
		releaseCapture();
		if (!m_handlePart.expired()) {
			std::unique_ptr<RedoState> redoState(new RedoState());
			saveState(redoState.get());
			XmlState* undo = undoState.release();
			XmlState* redo = redoState.release();
			IDataState& dataState = workspace->getDataState();
			const Name& name = getName();
			dataState.pushState(name.toString(), undo, redo);
			workspace->getDataState().setDirty(true);
		}
	}
	return nullptr;
}

void ResizeTool::resizeBy(int amount)
{
	if (!m_handlePart.expired()) {
		PartInstance* part = static_cast<PartInstance*>(m_handlePart.lock().get());
		part->destroyJoints();
		if (part->resize(m_handleFace, amount)) {
			ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(CLICK_SOUND);
		}
		part->join();
	}
}

void ResizeTool::onMouseMove(const UIEvent& uiEvent)
{
	if (!m_handlePart.expired()) {
		G3D::Vector3 axis = Math::getWorldNormal(m_handleFace, m_handlePart.lock()->getLocation());
		G3D::Ray handleRay = G3D::Ray::fromOriginAndDirection(m_handlePoint, axis);
		G3D::Ray mouseRay = getUnitMouseRay(uiEvent);
		G3D::Vector3 closest = Math::closestPointOnRay(handleRay, mouseRay);
		float distance = (closest - m_handlePoint).dot(handleRay.direction) - m_studsMoved;
		if (distance > 1.0f) {
			int amount = G3D::iRound(floor(distance));
			m_studsMoved += amount;
			resizeBy(amount);
		}
		if (distance < -1.0f) {
			int amount = -G3D::iRound(floor(-distance));
			m_studsMoved += amount;
			resizeBy(amount);
		}
	}
}

void ResizeTool::findHandle(const UIEvent& uiEvent)
{
	ServiceClient<Selection> selection(workspace);
	std::vector<std::shared_ptr<Instance>>::const_iterator it;
	for (it = selection->begin(); it != selection->end(); ++it) {
		if (PartInstance* part = dynamic_cast<PartInstance*>(it->get())) {
			G3D::Ray ray = getUnitMouseRay(uiEvent);
			if (hitTestHandles(part->getExtentsLocal(), part->getLocation(), ray, m_handlePoint, m_handleFace)) {
				m_handlePart = shared_from(part);
				return;
			}
		}
	}
	m_handlePart.reset();
}

void ResizeTool::render3dAdorn(Adorn* adorn)
{
	ServiceClient<Selection> selection(workspace);
	std::vector<std::shared_ptr<Instance>>::const_iterator it;
	for (it = selection->begin(); it != selection->end(); ++it) {
		if (PartInstance* part = dynamic_cast<PartInstance*>(it->get())) {
			DrawAdorn::handles3d(
				part->getPartSizeXml(),
				part->getCoordinateFrame(),
				adorn,
				HANDLE_RESIZE,
				workspace->getGCamera().getCoordinateFrame().translation
			);
		}
	}
}

void ResizeTool::render2d(Adorn* adorn)
{
	ServiceClient<Selection> selection(workspace);
	std::vector<std::shared_ptr<Instance>>::const_iterator it;
	for (it = selection->begin(); it != selection->end(); ++it) {
		if (PartInstance* part = dynamic_cast<PartInstance*>(it->get())) {
			DrawAdorn::handles2d(
				part->getPartSizeXml(),
				part->getCoordinateFrame(),
				workspace->getGCamera(),
				adorn,
				DrawAdorn::resizeColor()
			);
		}
	}
}

void ResizeTool::onMouseHover(const UIEvent& uiEvent)
{
	findHandle(uiEvent);
	m_overHandle = !m_handlePart.expired();
	ArrowToolBase::onMouseHover(uiEvent);
}

MouseCommand* ResizeTool::onMouseDown(const UIEvent& uiEvent)
{
	findHandle(uiEvent);
	if (m_handlePart.expired()) {
		return ArrowToolBase::onMouseDown(uiEvent);
	}

	m_mouseDownPosition = uiEvent.mousePosition;
	m_studsMoved = 0;
	m_unk0x48 = 0;
	m_overHandle = true;
	capture();
	undoState.reset(new UndoState());
	saveState(undoState.get());
	return this;
}

const std::string HammerTool::getCursorName() const
{
	return part ? "HammerOverCursor" : "HammerCursor";
}

void HammerTool::render3dAdorn(Adorn* adorn)
{
	if (part) {
		part->render3dSelect(adorn, SELECT_NORMAL);
	}
}

void HammerTool::onMouseIdle(const UIEvent& uiEvent)
{
	part = shared_from(getUnlockedPartByLocalCharacter(uiEvent));
}

MouseCommand* HammerTool::onMouseDown(const UIEvent& uiEvent)
{
	if (part) {
		std::shared_ptr<Explosion> explosion = Creatable<Instance>::create<Explosion>();
		Explosion::propPosition.setValue(explosion.get(), part->getCoordinateFrame().translation);
		Explosion::propBlastPressure.setValue(explosion.get(), 0.0f);
		explosion->setParent(workspace);
		part->setParent(nullptr);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(BOMB_SOUND);
	}
	return nullptr;
}

void LaserTool::render3dAdorn(Adorn* adorn)
{
	if (m_beamFrames > 0) {
		if (!FrameInterpolator::isExtraFrame()) {
			m_beamFrames--;
		}
		adorn->setObjectToWorldMatrix(G3D::CoordinateFrame());
		adorn->lineSegment(G3D::LineSegment::fromTwoPoints(m_beamStart, m_beamEnd), G3D::Color3::red(), 5.0f);
	}
}

MouseCommand* LaserTool::onMouseDown(const UIEvent& uiEvent)
{
	PartInstance* head = Humanoid::getLocalHeadFromContext(workspace);
	if (head) {
		m_beamStart = head->getCoordinateFrame().translation;
		G3D::Vector3 hitWorld;
		PartInstance* part = getPart(uiEvent, nullptr, hitWorld);
		if (part) {
			std::shared_ptr<Explosion> explosion = Creatable<Instance>::create<Explosion>();
			Explosion::propPosition.setValue(explosion.get(), hitWorld);
			Explosion::propBlastPressure.setValue(explosion.get(), 0.0f);
			explosion->setParent(workspace);
			if (!part->getPartLocked()) {
				part->remove();
			}
			ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(SNAP_SOUND);
		}
		m_beamFrames = 4;
	}
	return this;
}

void GrabTool::onMouseHover(const UIEvent& uiEvent)
{
	PartInstance* part = getUnlockedPartByLocalCharacter(uiEvent);
	if (!part) {
		cursor = "ArrowFarCursor";
	}
	else if (part->getPartLocked()) {
		cursor = "ArrowCursor";
	}
	else {
		cursor = "DragCursor";
	}
}

GrabTool::~GrabTool() = default;

GrabTool::GrabTool(Workspace* workspace) : Named<MouseCommand, sGrabTool>(workspace), cursor("ArrowCursor")
{
}

MouseCommand* GrabTool::onMouseDown(const UIEvent& uiEvent)
{
	G3D::Vector3 hitWorld;
	PartInstance* part = getUnlockedPartByLocalCharacter(uiEvent, hitWorld);
	if (part) {
		std::vector<Instance*> dragInstances;
		dragInstances.push_back(getTopSelectable3d(part));
		return DragTool::onMouseDown(part, hitWorld, dragInstances, uiEvent, workspace);
	}
	return nullptr;
}

const std::string CloneTool::getCursorName() const
{
	if (part) {
		return "CloneOverCursor";
	}
	return "CloneCursor";
}

CloneTool::CloneTool(Workspace* workspace) : Named<MouseCommand, sCloneTool>(workspace)
{
}

MouseCommand* CloneTool::onMouseDown(const UIEvent& uiEvent)
{
	if (part) {
		const G3D::CoordinateFrame& cframe = part->getCoordinateFrame();
		std::shared_ptr<PartInstance> clone = std::static_pointer_cast<PartInstance>(part->clone());
		std::vector<std::shared_ptr<Instance>> instances;
		instances.push_back(clone);
		workspace->insertInstances(instances, workspace, INSERT_TO_TREE, SUPPRESS_PROMPTS);
		ServiceProvider::create<Soundscape::SoundService>(workspace)->playSound(PING_SOUND);

		MouseCommand* dragger = new PartDragTool(clone.get(), clone->getCoordinateFrame().translation, workspace);
		return dragger->onMouseDown(uiEvent);
	}
	return nullptr;
}

void CloneTool::onMouseIdle(const UIEvent& uiEvent)
{
	part = shared_from(getUnlockedPartByLocalCharacter(uiEvent));
}

bool GameTool::canInteract(PartInstance* part, const G3D::Vector3& hitWorld) const
{
	return !part->getPartLocked() && !part->lockedInPlace() && characterCanReach(hitWorld);
}

void GameTool::onMouseHover(const UIEvent& uiEvent)
{
	if (uiEvent.windowSize.x != 0 && uiEvent.windowSize.y != 0) {
		G3D::Vector3 hitWorld;
		PartInstance* part = getUnlockedPartByLocalCharacter(uiEvent, hitWorld);
		if (!part) {
			cursor = "ArrowFarCursor";
		}
		else if (!canInteract(part, hitWorld)) {
			cursor = "ArrowCursor";
		}
		else {
			cursor = "DragCursor";
		}
	}
}

MouseCommand* GameTool::onMouseDown(const UIEvent& uiEvent)
{
	G3D::Vector3 hitWorld;
	PartInstance* part = getUnlockedPartByLocalCharacter(uiEvent, hitWorld);
	if (part && canInteract(part, hitWorld)) {
		MouseCommand* dragger = new PartDragTool(part, hitWorld, workspace);
		return dragger->onMouseDown(uiEvent);
	}
	return nullptr;
}

GameTool::~GameTool() = default;

GameTool::GameTool(Workspace* workspace) : Named<MouseCommand, sGameTool>(workspace)
{
}

AxisMoveTool::AxisMoveTool(Workspace* workspace)
	: Named<ArrowToolBase, sAxisMoveTool>(workspace), cursor("ArrowCursor"), m_dragging(false)
{
}

MouseCommand* AxisMoveTool::onMouseUp(const UIEvent& uiEvent)
{
	if (m_dragging) {
		dragger->finishDragging();
		dragger.release();
		m_dragging = false;
	}
	releaseCapture();
	return nullptr;
}

void AxisMoveTool::onMouseMove(const UIEvent& uiEvent)
{
	if (!m_dragging) {
		if ((G3D::Vector2(uiEvent.mousePosition) - G3D::Vector2(m_mouseDownPosition)).length() > 4.0f) {
			ServiceClient<FilteredSelection<PVInstance>> selection(workspace);
			dragger.reset(new MegaDragger(nullptr, selection->items(), workspace));
			dragger->startDragging();
			dragger->alignAndCleanParts();
			m_dragging = true;
		}
		if (!m_dragging) {
			return;
		}
	}

	if (dragger->anyDragPartAlive()) {
		G3D::Vector3 closest = Math::closestPointOnRay(m_handleRay, getUnitMouseRay(uiEvent));
		closest = Math::toGrid(closest, snapGrid());
		G3D::Vector3 delta = Math::toGrid(closest - m_position, snapGrid());
		if (delta != G3D::Vector3::zero()) {
			dragger->continueDragging();
			G3D::Vector3 moved = dragger->safeMoveAlongLine(delta);
			if (moved != G3D::Vector3::zero()) {
				m_position = Math::toGrid(m_position + moved, snapGrid());
			}
		}
	}
}

bool AxisMoveTool::computeSelectionExtents(Extents& extents)
{
	ServiceClient<Selection> selection(workspace);
	std::vector<Primitive*> primitives;
	std::for_each(selection->begin(), selection->end(), [&primitives](const std::shared_ptr<Instance>& instance) {
		DragUtilities::getPrimitives2(instance, primitives);
	});
	if (primitives.size() != 0) {
		extents = Dragger::computeExtents(primitives);
		return true;
	}

	return false;
}

bool AxisMoveTool::hitTestSelectionHandles(const UIEvent& uiEvent, G3D::Vector3& hitPoint, NormalId& normalId)
{
	Extents extents;
	if (computeSelectionExtents(extents)) {
		if (hitTestExtentsHandles(extents, getUnitMouseRay(uiEvent), hitPoint, normalId)) {
			return true;
		}
	}
	return false;
}

MouseCommand* AxisMoveTool::onMouseDown(const UIEvent& uiEvent)
{
	G3D::Vector3 hitPoint;
	NormalId normalId;
	if (hitTestSelectionHandles(uiEvent, hitPoint, normalId)) {
		m_mouseDownPosition = uiEvent.mousePosition;
		m_axis = normalId % 3;
		m_position = Math::toGrid(hitPoint, snapGrid());
		m_handleRay = G3D::Ray::fromOriginAndDirection(m_position, normalIdToVector3(normalId));
		capture();
		return this;
	}

	return ArrowToolBase::onMouseDown(uiEvent);
}

void AxisMoveTool::render2d(Adorn* adorn)
{
	Extents extents;
	if (computeSelectionExtents(extents)) {
		CoordinateFrame location(extents.center());
		G3D::Vector3 size = extents.size();
		DrawAdorn::handles2d(size, location, workspace->getGCamera(), adorn, G3D::Color3::orange());
	}
}

void AxisMoveTool::render3dAdorn(Adorn* adorn)
{
	Extents extents;
	if (computeSelectionExtents(extents)) {
		CoordinateFrame location(extents.center());
		G3D::Vector3 size = extents.size();
		DrawAdorn::handles3d(
			size, location, adorn, HANDLE_MOVE, workspace->getGCamera().getCoordinateFrame().translation
		);
	}
}

void AxisMoveTool::onMouseIdle(const UIEvent& uiEvent)
{
	ArrowToolBase::onMouseIdle(uiEvent);
	if (captured()) {
		cursor = "DragCursor";
		return;
	}

	G3D::Vector3 hitPoint;
	NormalId normalId;
	if (hitTestSelectionHandles(uiEvent, hitPoint, normalId)) {
		cursor = "ResizeCursor";
	}
	else {
		cursor = ArrowToolBase::getCursorName();
	}
}

UndoVerb::UndoVerb(DataModel* dataModel) : Verb(dataModel, "Undo"), m_dataModel(dataModel)
{
}

RedoVerb::RedoVerb(DataModel* dataModel) : Verb(dataModel, "Redo"), m_dataModel(dataModel)
{
}

bool UndoVerb::isEnabled() const
{
	std::string name;
	return m_dataModel->canUndo(name);
}

void UndoVerb::doIt(IDataState* dataState)
{
	SerializerV2 serializer;
	serializer.merge(m_dataModel->undo()->getData(), m_dataModel);
}

bool RedoVerb::isEnabled() const
{
	std::string name;
	return m_dataModel->canRedo(name);
}

void RedoVerb::doIt(IDataState* dataState)
{
	SerializerV2 serializer;
	serializer.merge(m_dataModel->redo()->getData(), m_dataModel);
}

} // namespace RBX
