#include "materials/MaterialUtils.h"

#include "utils/MArrayWrapper.h"
#include "utils/MELScriptBuilder.h"
#include "utils/MItDependencyNodesWrapper.h"
#include "utils/MayaUtilities.h"
#include "utils/Utilities.h"

#include "PRTContext.h"

#include "maya/MDataBlock.h"
#include "maya/MDataHandle.h"
#include "maya/MFnMesh.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MPlugArray.h"
#include "maya/adskDataAssociations.h"

namespace {
const MELVariable MEL_UNDO_STATE(L"materialUndoState");

constexpr const wchar_t* RGBA8_FORMAT = L"RGBA8";
constexpr const wchar_t* FORMAT_STRING = L"format";

MObject findNamedObject(const std::wstring& name, MFn::Type fnType) {
	MStatus status;
	MItDependencyNodes nodeIt(fnType, &status);
	MCHECK(status);

	for (const auto& nodeObj : MItDependencyNodesWrapper(nodeIt)) {
		MFnDependencyNode node(nodeObj);
		if (std::wcscmp(node.name().asWChar(), name.c_str()) == 0)
			return nodeObj;
	}

	return MObject::kNullObj;
}

adsk::Data::Structure* createNewMaterialInfoMapStructure() {
	adsk::Data::Structure* fStructure;

	// Register our structure since it is not registered yet.
	fStructure = adsk::Data::Structure::create();
	fStructure->setName(PRT_MATERIALINFO_MAP_STRUCTURE.c_str());

	fStructure->addMember(adsk::Data::Member::eDataType::kUInt64, 1, PRT_MATERIALINFO_MAP_KEY.c_str());
	fStructure->addMember(adsk::Data::Member::eDataType::kString, 1, PRT_MATERIALINFO_MAP_VALUE.c_str());

	adsk::Data::Structure::registerStructure(*fStructure);

	return fStructure;
}

} // namespace

namespace MaterialUtils {

void forwardGeometry(const MObject& aInMesh, const MObject& aOutMesh, MDataBlock& data) {
	MStatus status;

	const MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
	MCHECK(status);

	MDataHandle outMeshHandle = data.outputValue(aOutMesh, &status);
	MCHECK(status);

	status = outMeshHandle.set(inMeshHandle.asMesh());
	MCHECK(status);

	outMeshHandle.setClean();
}

adsk::Data::Stream* getMaterialStream(const MObject& aInMesh, MDataBlock& data) {
	MStatus status;

	const MDataHandle inMeshHandle = data.inputValue(aInMesh, &status);
	MCHECK(status);

	const MFnMesh inMesh(inMeshHandle.asMesh(), &status);
	MCHECK(status);

	const adsk::Data::Associations* inMetadata = inMesh.metadata(&status);
	MCHECK(status);
	if (inMetadata == nullptr)
		return nullptr;

	adsk::Data::Associations inAssociations(inMetadata);
	adsk::Data::Channel* inMatChannel = inAssociations.findChannel(PRT_MATERIAL_CHANNEL);
	if (inMatChannel == nullptr)
		return nullptr;

	return inMatChannel->findDataStream(PRT_MATERIAL_STREAM);
}

MStatus getMeshName(MString& meshName, const MPlug& plug) {
	MStatus status;
	bool searchEnded = false;

	for (MPlug curPlug = plug; !searchEnded;) {
		searchEnded = true;
		MPlugArray connectedPlugs;
		curPlug.connectedTo(connectedPlugs, false, true, &status);
		MCHECK(status);
		if (!connectedPlugs.length()) {
			return MStatus::kFailure;
		}
		for (const auto& connectedPlug : mu::makeMArrayConstWrapper(connectedPlugs)) {
			MFnDependencyNode connectedDepNode(connectedPlug.node(), &status);
			MCHECK(status);
			MObject connectedDepNodeObj = connectedDepNode.object(&status);
			MCHECK(status);
			if (connectedDepNodeObj.hasFn(MFn::kMesh)) {
				meshName = connectedDepNode.name(&status);
				MCHECK(status);
				break;
			}
			if (connectedDepNodeObj.hasFn(MFn::kGroupParts)) {
				curPlug = connectedDepNode.findPlug("outputGeometry", true, &status);
				MCHECK(status);
				searchEnded = false;
				break;
			}
		}
	}

	return MStatus::kSuccess;
}

MaterialCache getMaterialsByStructure(const adsk::Data::Structure& materialStructure, const std::wstring& baseName) {
	MaterialCache existingMaterialInfos;

	MStatus status;
	MItDependencyNodes shaderIt(MFn::kShadingEngine, &status);
	MCHECK(status);
	for (const auto& nodeObj : MItDependencyNodesWrapper(shaderIt)) {
		MFnDependencyNode node(nodeObj);

		const adsk::Data::Associations* materialMetadata = node.metadata(&status);
		MCHECK(status);

		if (materialMetadata == nullptr)
			continue;

		adsk::Data::Associations materialAssociations(materialMetadata);
		adsk::Data::Channel* matChannel = materialAssociations.findChannel(PRT_MATERIAL_CHANNEL);

		if (matChannel == nullptr)
			continue;

		adsk::Data::Stream* matStream = matChannel->findDataStream(PRT_MATERIAL_STREAM);
		if ((matStream == nullptr) || (matStream->elementCount() != 1))
			continue;

		adsk::Data::Handle matSHandle = matStream->element(0);
		if (!matSHandle.usesStructure(materialStructure))
			continue;

		if (std::wcsncmp(node.name().asWChar(), baseName.c_str(), baseName.length()) != 0)
			continue;

		existingMaterialInfos.emplace(matSHandle, node.name().asWChar());
	}

	return existingMaterialInfos;
}

bool getFaceRange(adsk::Data::Handle& handle, std::pair<int, int>& faceRange) {
	if (!handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_START.c_str()))
		return false;
	faceRange.first = *handle.asInt32();

	if (!handle.setPositionByMemberName(PRT_MATERIAL_FACE_INDEX_END.c_str()))
		return false;
	faceRange.second = *handle.asInt32();

	return true;
}

void assignMaterialMetadata(const adsk::Data::Structure& materialStructure, const adsk::Data::Handle& streamHandle,
                            const std::wstring& shadingEngineName) {
	MObject shadingEngineObj = findNamedObject(shadingEngineName, MFn::kShadingEngine);
	MFnDependencyNode shadingEngine(shadingEngineObj);

	adsk::Data::Associations newMetadata;
	adsk::Data::Channel newChannel = newMetadata.channel(PRT_MATERIAL_CHANNEL);
	adsk::Data::Stream newStream(materialStructure, PRT_MATERIAL_STREAM);
	newChannel.setDataStream(newStream);
	newMetadata.setChannel(newChannel);
	adsk::Data::Handle handle(streamHandle);
	handle.makeUnique();
	newStream.setElement(0, handle);
	shadingEngine.setMetadata(newMetadata);
}

std::wstring synchronouslyCreateShadingEngine(const std::wstring& desiredShadingEngineName,
                                              const MELVariable& shadingEngineVariable, MStatus& status) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.setVar(shadingEngineVariable, MELStringLiteral(desiredShadingEngineName));
	scriptBuilder.setsCreate(shadingEngineVariable);

	std::wstring output;
	status = scriptBuilder.executeSync(output);

	return output;
}

std::filesystem::path getStingrayShaderPath() {
	static const std::filesystem::path sfxFile = []() {
		const std::filesystem::path shaderPath =
		        (PRTContext::get().mPluginRootPath.parent_path() / L"shaders/serlioShaderStingray.sfx");
		LOG_DBG << "stingray shader located at " << shaderPath;
		return shaderPath;
	}();
	return sfxFile;
}

bool textureHasAlphaChannel(const std::wstring& path) {
	const AttributeMapUPtr textureMetadata(prt::createTextureMetadata(prtu::toFileURI(path).c_str()));
	if (textureMetadata == nullptr)
		return false;
	const wchar_t* format = textureMetadata->getString(FORMAT_STRING);
	if (format != nullptr && std::wcscmp(format, RGBA8_FORMAT) == 0)
		return true;
	return false;
}

void resetMaterial(const std::wstring& meshName) {
	MELScriptBuilder scriptBuilder;
	scriptBuilder.declInt(MEL_UNDO_STATE);
	scriptBuilder.getUndoState(MEL_UNDO_STATE);
	scriptBuilder.setUndoState(false);
	scriptBuilder.setsUseInitialShadingGroup(meshName);
	scriptBuilder.setUndoState(MEL_UNDO_STATE);
	scriptBuilder.execute();
}
} // namespace MaterialUtils
