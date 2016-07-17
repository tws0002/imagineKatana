#ifndef MATERIAL_HELPER_H
#define MATERIAL_HELPER_H

#include <map>
#include <vector>
#include <string>

#include <FnAttribute/FnAttribute.h>
#include <FnAttribute/FnDataBuilder.h>
#include <FnScenegraphIterator/FnScenegraphIterator.h>

#ifdef KAT_V_2
#include <FnRenderOutputUtils/FnRenderOutputUtils.h>
#else
#include <RenderOutputUtils/RenderOutputUtils.h>
#endif

#include "colour/colour3f.h"
#include "core/hash.h"

class Material;
class Texture;

class MaterialHelper
{
public:
	MaterialHelper();

	Material* getOrCreateMaterialForLocation(FnKat::FnScenegraphIterator iterator, const FnKat::GroupAttribute& imagineStatements);

	FnKat::GroupAttribute getMaterialForLocation(FnKat::FnScenegraphIterator iterator) const;

	std::vector<Material*>& getMaterialsVector() { return m_aMaterials; }

	Material* getDefaultMaterial() { return m_pDefaultMaterial; }

protected:
#ifndef KAT_V_2
	static std::string getMaterialHash(const FnKat::GroupAttribute& attribute);
#endif

	// this adds to the instances map itself if materials are created
	Material* createNewMaterial(const FnKat::GroupAttribute& attribute, bool isMatte);


protected:
	// called by createNewMaterial to try and create a network material from the attribute...
	Material* createNetworkMaterial(const FnKat::GroupAttribute& attribute, bool isMatte);

	static Texture* createNetworkOpItem(const std::string& opName, const FnKat::GroupAttribute& params);

	static void connectOpToMaterial(Material* pMaterial, const std::string& shaderName, const std::string& paramName, const Texture* pOp);
	static void connectOpToOp(Texture* pTargetOp, const std::string& opName, const std::string& paramName, const Texture* pSourceOp);

	// stuff for uber-shaders
	static Material* createStandardMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr,
											FnKat::GroupAttribute& alphaParamsAttr);
	static Material* createGlassMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createPlasticMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createBrushedMetalMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createMetallicPaintMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createTranslucentMaterial(const FnKat::GroupAttribute& shaderParamsAttr, FnKat::GroupAttribute& bumpParamsAttr);
	static Material* createVelvetMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createLuminousMaterial(const FnKat::GroupAttribute& shaderParamsAttr);
	static Material* createWireframeMaterial(const FnKat::GroupAttribute& shaderParamsAttr);

protected:
	FnKat::StringAttribute			m_terminatorNodes;

	std::map<HashValue, Material*>	m_aMaterialInstances; // all materials with hashes

	std::vector<Material*>			m_aMaterials; // all material instances in std::vector

	Material*						m_pDefaultMaterial;
	Material*						m_pDefaultMaterialMatte; // annoying, but...
};

#endif // MATERIAL_HELPER_H
