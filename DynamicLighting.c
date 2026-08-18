#include "src/PluginAPI.h"
#include "src/Block.h"
#include "src/World.h"
#include "src/Lighting.h"
#include "src/Entity.h"
#include "src/Inventory.h"
#include "src/PackedCol.h"
#include "src/Game.h"

#include <math.h>

#define DYNAMIC_LIGHT_RADIUS 6.0f
#define DYNAMIC_LIGHT_UPDATE_INTERVAL 0.02

static PackedCol (*Original_Color)(int x, int y, int z);
static PackedCol (*Original_Color_XSide)(int x, int y, int z);
static PackedCol (*Original_Color_Sprite_Fast)(int x, int y, int z);
static PackedCol (*Original_Color_YMax_Fast)(int x, int y, int z);
static PackedCol (*Original_Color_YMin_Fast)(int x, int y, int z);
static PackedCol (*Original_Color_XSide_Fast)(int x, int y, int z);
static PackedCol (*Original_Color_ZSide_Fast)(int x, int y, int z);

static Vec3 LastLightPosition;
static cc_bool HaveLastLightPosition = false;
static BlockID LastHeldBlock = BLOCK_AIR;
static cc_uint8 LastBrightness = 0;
static cc_bool DynamicLighting_Initialised = false;

static cc_uint8 DynamicLighting_GetLavaBrightness(BlockID block)
{
	if (block >= BLOCK_COUNT) return 0;
	return Blocks.Brightness[block] & 0x0F;
}

static cc_uint8 DynamicLighting_GetLampBrightness(BlockID block)
{
	if (block >= BLOCK_COUNT) return 0;
	return (Blocks.Brightness[block] >> 4) & 0x0F;
}

static cc_uint8 DynamicLighting_GetBrightness(void)
{
	BlockID block = Inventory_SelectedBlock;
	cc_uint8 lava, lamp;

	if (block >= BLOCK_COUNT) return 0;

	lava = DynamicLighting_GetLavaBrightness(block);
	lamp = DynamicLighting_GetLampBrightness(block);

	return lava > 0 ? lava : lamp;
}

static PackedCol DynamicLighting_GetLightColor(void)
{
	BlockID block = Inventory_SelectedBlock;
	cc_uint8 lava, lamp;

	if (block >= BLOCK_COUNT) return PACKEDCOL_WHITE;

	lava = DynamicLighting_GetLavaBrightness(block);
	lamp = DynamicLighting_GetLampBrightness(block);

	if (lava > 0) return Env.LavaLightCol;
	if (lamp > 0) return Env.LampLightCol;
	return PACKEDCOL_WHITE;
}

static Vec3 DynamicLighting_GetBlockShapeCenter(BlockID block)
{
	Vec3 center;

	center.x = (Blocks.RenderMinBB[block].x + Blocks.RenderMaxBB[block].x) * 0.5f;
	center.y = (Blocks.RenderMinBB[block].y + Blocks.RenderMaxBB[block].y) * 0.5f;
	center.z = (Blocks.RenderMinBB[block].z + Blocks.RenderMaxBB[block].z) * 0.5f;

	return center;
}

static Vec3 DynamicLighting_GetPosition(void)
{
	Vec3 pos;
	Vec3 shapeCenter;
	BlockID block;

	pos = Entities.CurPlayer->Base.Position;
	pos.y += 1.62f;

	block = Inventory_SelectedBlock;
	if (block < BLOCK_COUNT)
	{
		shapeCenter = DynamicLighting_GetBlockShapeCenter(block);
		pos.x += shapeCenter.x - 0.5f;
		pos.y += shapeCenter.y - 0.5f;
		pos.z += shapeCenter.z - 0.5f;
	}

	return pos;
}

static float DynamicLighting_GetRadius(void)
{
	cc_uint8 brightness = DynamicLighting_GetBrightness();

	if (brightness == 0) return 0.0f;
	return 7.0f * ((float)brightness / 15.0f);
}

static float DynamicLighting_Strength(int x, int y, int z)
{
	Vec3 pos;
	float dx, dy, dz, dist, distSq;
	float radius, distanceStrength, brightnessStrength;
	cc_uint8 brightness;

	if (!Entities.CurPlayer) return 0.0f;

	brightness = DynamicLighting_GetBrightness();
	if (brightness == 0) return 0.0f;

	pos = DynamicLighting_GetPosition();
	radius = DynamicLighting_GetRadius();

	dx = ((float)x + 0.5f) - pos.x;
	dy = ((float)y + 0.5f) - pos.y;
	dz = ((float)z + 0.5f) - pos.z;

	distSq = dx * dx + dy * dy + dz * dz;
	if (distSq >= radius * radius) return 0.0f;

	dist = sqrtf(distSq);
	distanceStrength = 1.0f - (dist / radius);
	if (distanceStrength <= 0.0f) return 0.0f;

	brightnessStrength = (float)brightness / 15.0f;
	if (brightnessStrength > 1.0f) brightnessStrength = 1.0f;

	return distanceStrength * brightnessStrength;
}

static PackedCol DynamicLighting_GetScaledLight(float strength)
{
	return PackedCol_Scale(DynamicLighting_GetLightColor(), strength);
}

static PackedCol DynamicLighting_Apply(PackedCol base, int x, int y, int z)
{
	float strength = DynamicLighting_Strength(x, y, z);
	PackedCol light;

	if (strength <= 0.0f) return base;

	light = DynamicLighting_GetScaledLight(strength);
	return PackedCol_ScreenBlend(base, light);
}

static PackedCol DynamicLighting_Color(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_XSide(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_XSide(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_Sprite_Fast(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_Sprite_Fast(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_YMax_Fast(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_YMax_Fast(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_YMin_Fast(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_YMin_Fast(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_XSide_Fast(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_XSide_Fast(x, y, z), x, y, z);
}

static PackedCol DynamicLighting_Color_ZSide_Fast(int x, int y, int z)
{
	return DynamicLighting_Apply(Original_Color_ZSide_Fast(x, y, z), x, y, z);
}

static void DynamicLighting_RebuildAround(Vec3 pos)
{
	int cx = (int)floorf(pos.x);
	int cy = (int)floorf(pos.y);
	int cz = (int)floorf(pos.z);
	int x, y, z;

	for (x = cx - 8; x <= cx + 8; x += 8)
	{
		for (y = cy - 8; y <= cy + 8; y += 8)
		{
			for (z = cz - 8; z <= cz + 8; z += 8)
			{
				if (!World_Contains(x, y, z)) continue;

				Game_UpdateBlock(x, y, z, World_GetBlock(x, y, z));
			}
		}
	}
}

static cc_bool DynamicLighting_PositionChanged(Vec3* oldPos, Vec3* newPos)
{
	Vec3 pos;
	float dx, dy, dz, distSq;

	if (!Entities.CurPlayer) return false;

	pos = Entities.CurPlayer->Base.Position;

	if (!HaveLastLightPosition)
	{
		*oldPos = pos;
		*newPos = pos;
		LastLightPosition = pos;
		HaveLastLightPosition = true;
		return true;
	}

	*oldPos = LastLightPosition;
	*newPos = pos;

	dx = pos.x - LastLightPosition.x;
	dy = pos.y - LastLightPosition.y;
	dz = pos.z - LastLightPosition.z;
	distSq = dx * dx + dy * dy + dz * dz;

	if (distSq < 0.0001f) return false;

	LastLightPosition = pos;
	return true;
}

static cc_bool DynamicLighting_BlockChanged(void)
{
	BlockID block = Inventory_SelectedBlock;
	cc_uint8 brightness;

	brightness = block >= BLOCK_COUNT ? 0 : Blocks.Brightness[block];

	if (block == LastHeldBlock && brightness == LastBrightness)
		return false;

	LastHeldBlock = block;
	LastBrightness = brightness;
	return true;
}

static void DynamicLighting_Update(void)
{
	Vec3 oldPos, newPos;
	cc_bool moved, blockChanged;

	if (!Entities.CurPlayer) return;

	moved = DynamicLighting_PositionChanged(&oldPos, &newPos);
	blockChanged = DynamicLighting_BlockChanged();

	if (!moved && !blockChanged) return;

	DynamicLighting_RebuildAround(oldPos);
	DynamicLighting_RebuildAround(newPos);

	DynamicLighting_RebuildAround((Vec3){ newPos.x - 8.0f, newPos.y, newPos.z });
	DynamicLighting_RebuildAround((Vec3){ newPos.x + 8.0f, newPos.y, newPos.z });
	DynamicLighting_RebuildAround((Vec3){ newPos.x, newPos.y, newPos.z - 8.0f });
	DynamicLighting_RebuildAround((Vec3){ newPos.x, newPos.y, newPos.z + 8.0f });
}

static void DynamicLighting_Task(struct ScheduledTask* task)
{
	(void)task;
	DynamicLighting_Update();
}

static void DynamicLighting_MapLoaded(void)
{
	LastHeldBlock = BLOCK_AIR;
	LastBrightness = 0;
	HaveLastLightPosition = false;
	DynamicLighting_Update();
}

static void DynamicLighting_Init(void)
{
	Original_Color = Lighting.Color;
	Original_Color_XSide = Lighting.Color_XSide;
	Original_Color_Sprite_Fast = Lighting.Color_Sprite_Fast;
	Original_Color_YMax_Fast = Lighting.Color_YMax_Fast;
	Original_Color_YMin_Fast = Lighting.Color_YMin_Fast;
	Original_Color_XSide_Fast = Lighting.Color_XSide_Fast;
	Original_Color_ZSide_Fast = Lighting.Color_ZSide_Fast;

	if (!Original_Color ||
		!Original_Color_XSide ||
		!Original_Color_Sprite_Fast ||
		!Original_Color_YMax_Fast ||
		!Original_Color_YMin_Fast ||
		!Original_Color_XSide_Fast ||
		!Original_Color_ZSide_Fast)
		return;

	Lighting.Color = DynamicLighting_Color;
	Lighting.Color_XSide = DynamicLighting_Color_XSide;
	Lighting.Color_Sprite_Fast = DynamicLighting_Color_Sprite_Fast;
	Lighting.Color_YMax_Fast = DynamicLighting_Color_YMax_Fast;
	Lighting.Color_YMin_Fast = DynamicLighting_Color_YMin_Fast;
	Lighting.Color_XSide_Fast = DynamicLighting_Color_XSide_Fast;
	Lighting.Color_ZSide_Fast = DynamicLighting_Color_ZSide_Fast;

	DynamicLighting_Initialised = true;
	LastHeldBlock = BLOCK_AIR;
	LastBrightness = 0;
	HaveLastLightPosition = false;

	ScheduledTask_Add(DYNAMIC_LIGHT_UPDATE_INTERVAL, DynamicLighting_Task);
	DynamicLighting_Update();
}

static void DynamicLighting_Free(void)
{
	if (!DynamicLighting_Initialised) return;

	Lighting.Color = Original_Color;
	Lighting.Color_XSide = Original_Color_XSide;
	Lighting.Color_Sprite_Fast = Original_Color_Sprite_Fast;
	Lighting.Color_YMax_Fast = Original_Color_YMax_Fast;
	Lighting.Color_YMin_Fast = Original_Color_YMin_Fast;
	Lighting.Color_XSide_Fast = Original_Color_XSide_Fast;
	Lighting.Color_ZSide_Fast = Original_Color_ZSide_Fast;

	DynamicLighting_Initialised = false;
}

PLUGIN_EXPORT int Plugin_ApiVersion = 1;

PLUGIN_EXPORT struct IGameComponent Plugin_Component = {
	DynamicLighting_Init,
	DynamicLighting_Free,
	NULL,
	NULL,
	DynamicLighting_MapLoaded
};