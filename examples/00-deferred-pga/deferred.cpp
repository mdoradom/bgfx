#include <bx/math.h>
#include <bx/timer.h>
#include <bx/uint32_t.h>
#include "common.h"
#include "bgfx_utils.h"
#include "logo.h"
#include "imgui/imgui.h"

namespace
{

// Deferred pipeline passes added incrementally in later milestones.
enum RenderPass : bgfx::ViewId
{
	RENDER_PASS_GEOMETRY = 0,
	RENDER_PASS_LIGHT = 1,
	RENDER_PASS_COMBINE = 2,
	RENDER_PASS_DEBUG = 3,
	RENDER_PASS_SHADOW = 4,
};

struct PosNormalTexcoordVertex
{
	float m_x;
	float m_y;
	float m_z;
	uint32_t m_normal;
	int16_t m_u;
	int16_t m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Int16, true, true)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosNormalTexcoordVertex::ms_layout;

struct PosTexCoord0Vertex
{
	float m_x;
	float m_y;
	float m_z;
	float m_u;
	float m_v;

	static void init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosTexCoord0Vertex::ms_layout;

static PosNormalTexcoordVertex s_cubeVertices[24] =
{
	{-1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f),      0,      0 },
	{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0x7fff,      0 },
	{-1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f),      0, 0x7fff },
	{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f,  0.0f,  1.0f), 0x7fff, 0x7fff },
	{-1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f),      0,      0 },
	{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0x7fff,      0 },
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f),      0, 0x7fff },
	{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f,  0.0f, -1.0f), 0x7fff, 0x7fff },
	{-1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f),      0,      0 },
	{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0x7fff,      0 },
	{-1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f),      0, 0x7fff },
	{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 0.0f,  1.0f,  0.0f), 0x7fff, 0x7fff },
	{-1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f),      0,      0 },
	{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0x7fff,      0 },
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f),      0, 0x7fff },
	{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 0.0f, -1.0f,  0.0f), 0x7fff, 0x7fff },
	{ 1.0f, -1.0f,  1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f),      0,      0 },
	{ 1.0f,  1.0f,  1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0x7fff,      0 },
	{ 1.0f, -1.0f, -1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f),      0, 0x7fff },
	{ 1.0f,  1.0f, -1.0f, encodeNormalRgba8( 1.0f,  0.0f,  0.0f), 0x7fff, 0x7fff },
	{-1.0f, -1.0f,  1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f),      0,      0 },
	{-1.0f,  1.0f,  1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0x7fff,      0 },
	{-1.0f, -1.0f, -1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f),      0, 0x7fff },
	{-1.0f,  1.0f, -1.0f, encodeNormalRgba8(-1.0f,  0.0f,  0.0f), 0x7fff, 0x7fff },
};

static const uint16_t s_cubeIndices[36] =
{
	 0,  2,  1,
	 1,  2,  3,
	 4,  5,  6,
	 5,  7,  6,

	 8, 10,  9,
	 9, 10, 11,
	12, 13, 14,
	13, 15, 14,

	16, 18, 17,
	17, 18, 19,
	20, 21, 22,
	21, 23, 22,
};

void screenSpaceQuad(bool _originBottomLeft)
{
	if (3 == bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_layout) )
	{
		bgfx::TransientVertexBuffer vb;
		bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_layout);
		PosTexCoord0Vertex* vertex = (PosTexCoord0Vertex*)vb.data;

		const float minx = -1.0f;
		const float maxx =  1.0f;
		const float miny =  0.0f;
		const float maxy =  2.0f;

		float minv = 0.0f;
		float maxv = 2.0f;
		if (_originBottomLeft)
		{
			const float tmp = minv;
			minv = maxv - 1.0f;
			maxv = tmp + 1.0f;
		}

		vertex[0].m_x = minx;
		vertex[0].m_y = miny;
		vertex[0].m_z = 0.0f;
		vertex[0].m_u = -1.0f;
		vertex[0].m_v = minv;

		vertex[1].m_x = maxx;
		vertex[1].m_y = miny;
		vertex[1].m_z = 0.0f;
		vertex[1].m_u =  1.0f;
		vertex[1].m_v = minv;

		vertex[2].m_x = maxx;
		vertex[2].m_y = maxy;
		vertex[2].m_z = 0.0f;
		vertex[2].m_u =  1.0f;
		vertex[2].m_v = maxv;

		bgfx::setVertexBuffer(0, &vb);
	}
}

class Deferred : public entry::AppI
{
public:
	Deferred(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_width(0)
		, m_height(0)
		, m_debug(0)
		, m_reset(0)
		, m_supportedBackend(false)
		, m_deferredSupported(false)
		, m_enableShadows(true)
		, m_enableIBL(true)
		, m_showGBuffer(false)
		, m_showShadowMap(false)
		, m_caps(NULL)
		, m_oldWidth(0)
		, m_oldHeight(0)
		, m_oldReset(0)
		, m_shadowResolution(2048)
		, m_shadowBias(0.0015f)
		, m_shadowNormalBias(0.02f)
		, m_directionalIntensity(4.0f)
		, m_ambientIblIntensity(1.0f)
	{
		m_vbh.idx = bgfx::kInvalidHandle;
		m_ibh.idx = bgfx::kInvalidHandle;
		s_texColor.idx = bgfx::kInvalidHandle;
		s_albedo.idx = bgfx::kInvalidHandle;
		s_light.idx = bgfx::kInvalidHandle;
		s_normal.idx = bgfx::kInvalidHandle;
		s_depth.idx = bgfx::kInvalidHandle;
		s_shadowMap.idx = bgfx::kInvalidHandle;
		u_lightDirIntensity.idx = bgfx::kInvalidHandle;
		u_lightAmbient.idx = bgfx::kInvalidHandle;
		u_lightMtx.idx = bgfx::kInvalidHandle;
		u_shadowParams.idx = bgfx::kInvalidHandle;
		u_invViewProj.idx = bgfx::kInvalidHandle;
		m_geomProgram.idx = bgfx::kInvalidHandle;
		m_lightProgram.idx = bgfx::kInvalidHandle;
		m_combineProgram.idx = bgfx::kInvalidHandle;
		m_debugProgram.idx = bgfx::kInvalidHandle;
		m_shadowProgram.idx = bgfx::kInvalidHandle;
		m_textureColor.idx = bgfx::kInvalidHandle;
		m_gbufferTex[0].idx = bgfx::kInvalidHandle;
		m_gbufferTex[1].idx = bgfx::kInvalidHandle;
		m_gbufferTex[2].idx = bgfx::kInvalidHandle;
		m_lightBufferTex.idx = bgfx::kInvalidHandle;
		m_shadowMapTex.idx = bgfx::kInvalidHandle;
		m_gbuffer.idx = bgfx::kInvalidHandle;
		m_lightBuffer.idx = bgfx::kInvalidHandle;
		m_shadowMapFB.idx = bgfx::kInvalidHandle;
	}

	void destroyFrameBuffers()
	{
		if (bgfx::isValid(m_gbuffer) )
		{
			bgfx::destroy(m_gbuffer);
		}
		m_gbuffer.idx = bgfx::kInvalidHandle;
		m_gbufferTex[0].idx = bgfx::kInvalidHandle;
		m_gbufferTex[1].idx = bgfx::kInvalidHandle;
		m_gbufferTex[2].idx = bgfx::kInvalidHandle;

		if (bgfx::isValid(m_lightBuffer) )
		{
			bgfx::destroy(m_lightBuffer);
		}
		m_lightBuffer.idx = bgfx::kInvalidHandle;
		m_lightBufferTex.idx = bgfx::kInvalidHandle;

		if (bgfx::isValid(m_shadowMapFB) )
		{
			bgfx::destroy(m_shadowMapFB);
		}
		m_shadowMapFB.idx = bgfx::kInvalidHandle;
		m_shadowMapTex.idx = bgfx::kInvalidHandle;
	}

	void createFrameBuffers()
	{
		destroyFrameBuffers();

		const uint64_t samplerFlags = 0
			| BGFX_SAMPLER_MIN_POINT
			| BGFX_SAMPLER_MAG_POINT
			| BGFX_SAMPLER_MIP_POINT
			| BGFX_SAMPLER_U_CLAMP
			| BGFX_SAMPLER_V_CLAMP
			;

		bgfx::Attachment gbufferAt[3];
		m_gbufferTex[0] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | samplerFlags);
		m_gbufferTex[1] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | samplerFlags);

		const bgfx::TextureFormat::Enum depthFormat =
			bgfx::isTextureValid(0, false, 1, bgfx::TextureFormat::D32F, BGFX_TEXTURE_RT | samplerFlags)
			? bgfx::TextureFormat::D32F
			: bgfx::TextureFormat::D24;

		m_gbufferTex[2] = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, BGFX_TEXTURE_RT | samplerFlags);

		gbufferAt[0].init(m_gbufferTex[0]);
		gbufferAt[1].init(m_gbufferTex[1]);
		gbufferAt[2].init(m_gbufferTex[2]);
		m_gbuffer = bgfx::createFrameBuffer(BX_COUNTOF(gbufferAt), gbufferAt, true);

		m_lightBufferTex = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT | samplerFlags);
		m_lightBuffer = bgfx::createFrameBuffer(1, &m_lightBufferTex, true);

		const uint64_t shadowSamplerFlags = 0
			| BGFX_SAMPLER_U_CLAMP
			| BGFX_SAMPLER_V_CLAMP
			| BGFX_SAMPLER_COMPARE_LESS
			;
		m_shadowMapTex = bgfx::createTexture2D(m_shadowResolution, m_shadowResolution, false, 1, bgfx::TextureFormat::D16, BGFX_TEXTURE_RT | shadowSamplerFlags);
		m_shadowMapFB = bgfx::createFrameBuffer(1, &m_shadowMapTex, true);
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_TEXT;
		m_reset  = BGFX_RESET_VSYNC;

		bgfx::Init init;
		init.type     = args.m_type;
		init.vendorId = args.m_pciId;
		init.platformData.nwh  = entry::getNativeWindowHandle(entry::kDefaultWindowHandle);
		init.platformData.ndt  = entry::getNativeDisplayHandle();
		init.platformData.type = entry::getNativeWindowHandleType();
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		m_caps = bgfx::getCaps();
		m_supportedBackend = m_caps->rendererType == bgfx::RendererType::OpenGL
			|| m_caps->rendererType == bgfx::RendererType::Vulkan;
		m_deferredSupported = m_caps->limits.maxFBAttachments >= 3;

		m_oldWidth = m_width;
		m_oldHeight = m_height;
		m_oldReset = m_reset;

		bgfx::setViewName(RENDER_PASS_GEOMETRY, "Geometry");
		bgfx::setViewName(RENDER_PASS_LIGHT, "Light");
		bgfx::setViewName(RENDER_PASS_COMBINE, "Combine");
		bgfx::setViewName(RENDER_PASS_DEBUG, "Debug");
		bgfx::setViewName(RENDER_PASS_SHADOW, "Shadow");

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(RENDER_PASS_GEOMETRY
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		bgfx::setViewClear(RENDER_PASS_LIGHT
			, BGFX_CLEAR_COLOR
			, 0xffffffff
			, 1.0f
			, 0
			);

		PosNormalTexcoordVertex::init();
		PosTexCoord0Vertex::init();

		m_vbh = bgfx::createVertexBuffer(bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices) ), PosNormalTexcoordVertex::ms_layout);
		m_ibh = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices) ) );

		s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
		s_albedo = bgfx::createUniform("s_albedo", bgfx::UniformType::Sampler);
		s_light = bgfx::createUniform("s_light", bgfx::UniformType::Sampler);
		s_normal = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
		s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);
		s_shadowMap = bgfx::createUniform("s_shadowMap", bgfx::UniformType::Sampler);
		u_lightDirIntensity = bgfx::createUniform("u_lightDirIntensity", bgfx::UniformType::Vec4);
		u_lightAmbient = bgfx::createUniform("u_lightAmbient", bgfx::UniformType::Vec4);
		u_lightMtx = bgfx::createUniform("u_lightMtx", bgfx::UniformType::Mat4);
		u_shadowParams = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
		u_invViewProj = bgfx::createUniform("u_invViewProjGeom", bgfx::UniformType::Mat4);

		m_geomProgram = loadProgram("vs_deferred_geom", "fs_deferred_geom");
		m_lightProgram = loadProgram("vs_deferred_light", "fs_deferred_light");
		m_combineProgram = loadProgram("vs_deferred_combine", "fs_deferred_combine");
		m_debugProgram = loadProgram("vs_deferred_debug", "fs_deferred_debug");
		m_shadowProgram = loadProgram("vs_deferred_shadow", "fs_deferred_shadow");

		m_textureColor = loadTexture("textures/fieldstone-rgba.dds");

		if (m_supportedBackend && m_deferredSupported)
		{
			createFrameBuffers();
		}

		imguiCreate();
	}

	virtual int shutdown() override
	{
		destroyFrameBuffers();

		if (bgfx::isValid(m_textureColor) )
		{
			bgfx::destroy(m_textureColor);
		}

		if (bgfx::isValid(m_shadowProgram) )
		{
			bgfx::destroy(m_shadowProgram);
		}

		if (bgfx::isValid(m_debugProgram) )
		{
			bgfx::destroy(m_debugProgram);
		}

		if (bgfx::isValid(m_combineProgram) )
		{
			bgfx::destroy(m_combineProgram);
		}

		if (bgfx::isValid(m_lightProgram) )
		{
			bgfx::destroy(m_lightProgram);
		}

		if (bgfx::isValid(m_geomProgram) )
		{
			bgfx::destroy(m_geomProgram);
		}

		if (bgfx::isValid(u_invViewProj) )
		{
			bgfx::destroy(u_invViewProj);
		}

		if (bgfx::isValid(u_shadowParams) )
		{
			bgfx::destroy(u_shadowParams);
		}

		if (bgfx::isValid(u_lightMtx) )
		{
			bgfx::destroy(u_lightMtx);
		}

		if (bgfx::isValid(s_shadowMap) )
		{
			bgfx::destroy(s_shadowMap);
		}

		if (bgfx::isValid(s_light) )
		{
			bgfx::destroy(s_light);
		}

		if (bgfx::isValid(s_depth) )
		{
			bgfx::destroy(s_depth);
		}

		if (bgfx::isValid(s_normal) )
		{
			bgfx::destroy(s_normal);
		}

		if (bgfx::isValid(u_lightAmbient) )
		{
			bgfx::destroy(u_lightAmbient);
		}

		if (bgfx::isValid(u_lightDirIntensity) )
		{
			bgfx::destroy(u_lightDirIntensity);
		}

		if (bgfx::isValid(s_albedo) )
		{
			bgfx::destroy(s_albedo);
		}

		if (bgfx::isValid(s_texColor) )
		{
			bgfx::destroy(s_texColor);
		}

		if (bgfx::isValid(m_ibh) )
		{
			bgfx::destroy(m_ibh);
		}

		if (bgfx::isValid(m_vbh) )
		{
			bgfx::destroy(m_vbh);
		}

		imguiDestroy();

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			const bool resized = m_oldWidth != m_width || m_oldHeight != m_height || m_oldReset != m_reset;
			if (resized && m_supportedBackend && m_deferredSupported)
			{
				createFrameBuffers();
			}

			if (resized)
			{
				m_oldWidth = m_width;
				m_oldHeight = m_height;
				m_oldReset = m_reset;
			}

			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT   : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT  : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);
			ImGui::SetNextWindowPos(ImVec2(10.0f, 100.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(360.0f, 290.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Deferred Debug", NULL, 0);
			ImGui::Text("Backend support: %s", m_supportedBackend ? "OpenGL/Vulkan path enabled" : "Fallback mode");
			ImGui::Checkbox("Enable Shadows", &m_enableShadows);
			ImGui::Checkbox("Enable IBL", &m_enableIBL);
			ImGui::Checkbox("Show GBuffer", &m_showGBuffer);
			ImGui::Checkbox("Show Shadow Map", &m_showShadowMap);
			const char* shadowResItems[] = { "1024", "2048", "4096" };
			int32_t shadowResIndex = m_shadowResolution == 1024 ? 0 : (m_shadowResolution == 4096 ? 2 : 1);
			if (ImGui::Combo("Shadow Resolution", &shadowResIndex, shadowResItems, BX_COUNTOF(shadowResItems) ) )
			{
				m_shadowResolution = shadowResIndex == 0 ? 1024 : (shadowResIndex == 2 ? 4096 : 2048);
				createFrameBuffers();
			}
			ImGui::SliderFloat("Shadow Bias", &m_shadowBias, 0.0001f, 0.01f, "%.4f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Normal Bias", &m_shadowNormalBias, 0.0f, 0.2f, "%.4f");
			ImGui::SliderFloat("Directional Intensity", &m_directionalIntensity, 0.0f, 10.0f, "%.2f");
			ImGui::SliderFloat("Ambient IBL Intensity", &m_ambientIblIntensity, 0.0f, 5.0f, "%.2f");
			ImGui::End();

			imguiEndFrame();

			const bool deferredReady = m_supportedBackend
				&& m_deferredSupported
				&& bgfx::isValid(m_gbuffer)
				&& bgfx::isValid(m_lightBuffer)
				&& bgfx::isValid(m_shadowMapFB)
				&& bgfx::isValid(m_geomProgram)
				&& bgfx::isValid(m_lightProgram)
				&& bgfx::isValid(m_combineProgram)
				&& bgfx::isValid(m_debugProgram)
				&& bgfx::isValid(m_shadowProgram)
				&& bgfx::isValid(m_vbh)
				&& bgfx::isValid(m_ibh)
				&& bgfx::isValid(m_textureColor);

			if (deferredReady)
			{
				float view[16];
				float proj[16];
				const bx::Vec3 eye = { 0.0f, 6.0f, -18.0f };
				const bx::Vec3 at = { 0.0f, 0.0f, 0.0f };
				bx::mtxLookAt(view, eye, at);
				bx::mtxProj(proj, 60.0f, float(m_width) / float(m_height), 0.1f, 100.0f, m_caps->homogeneousDepth);

				float ortho[16];
				bx::mtxOrtho(ortho, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1000.0f, 0.0f, m_caps->homogeneousDepth);

				const bx::Vec3 lightDir = bx::normalize(bx::Vec3{ -0.35f, -1.0f, -0.2f });

				float lightView[16];
				float lightProj[16];
				const bx::Vec3 lightEye = bx::mul(lightDir, -20.0f);
				const bx::Vec3 lightAt = { 0.0f, 0.0f, 0.0f };
				bx::mtxLookAt(lightView, lightEye, lightAt);
				bx::mtxOrtho(lightProj, -10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 50.0f, 0.0f, m_caps->homogeneousDepth);

				bgfx::setViewRect(RENDER_PASS_SHADOW, 0, 0, m_shadowResolution, m_shadowResolution);
				bgfx::setViewRect(RENDER_PASS_GEOMETRY, 0, 0, uint16_t(m_width), uint16_t(m_height) );
				bgfx::setViewRect(RENDER_PASS_LIGHT, 0, 0, uint16_t(m_width), uint16_t(m_height) );
				bgfx::setViewRect(RENDER_PASS_COMBINE, 0, 0, uint16_t(m_width), uint16_t(m_height) );
				bgfx::setViewRect(RENDER_PASS_DEBUG, 0, 0, uint16_t(m_width), uint16_t(m_height) );

				bgfx::setViewFrameBuffer(RENDER_PASS_SHADOW, m_shadowMapFB);
				bgfx::setViewFrameBuffer(RENDER_PASS_GEOMETRY, m_gbuffer);
				bgfx::setViewFrameBuffer(RENDER_PASS_LIGHT, m_lightBuffer);
				bgfx::setViewFrameBuffer(RENDER_PASS_COMBINE, BGFX_INVALID_HANDLE);
				bgfx::setViewFrameBuffer(RENDER_PASS_DEBUG, BGFX_INVALID_HANDLE);

				bgfx::setViewTransform(RENDER_PASS_SHADOW, lightView, lightProj);
				bgfx::setViewTransform(RENDER_PASS_GEOMETRY, view, proj);
				bgfx::setViewTransform(RENDER_PASS_LIGHT, NULL, ortho);
				bgfx::setViewTransform(RENDER_PASS_COMBINE, NULL, ortho);
				bgfx::setViewTransform(RENDER_PASS_DEBUG, NULL, ortho);

				bgfx::setViewClear(RENDER_PASS_SHADOW, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

				const float time = float(double(bx::getHPCounter()) / double(bx::getHPFrequency()));
				const uint32_t dim = 5;
				const float offset = float(dim - 1) * 1.8f * 0.5f;
				for (uint32_t yy = 0; yy < dim; ++yy)
				{
					for (uint32_t xx = 0; xx < dim; ++xx)
					{
						float mtx[16];
						bx::mtxRotateXY(mtx, time * 0.37f + xx * 0.21f, time * 0.23f + yy * 0.37f);
						mtx[12] = -offset + float(xx) * 1.8f;
						mtx[13] = -offset + float(yy) * 1.8f;
						mtx[14] = 0.0f;

						bgfx::setTransform(mtx);
						bgfx::setVertexBuffer(0, m_vbh);
						bgfx::setIndexBuffer(m_ibh);
						bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
						bgfx::submit(RENDER_PASS_SHADOW, m_shadowProgram);

						bgfx::setTransform(mtx);
						bgfx::setTexture(0, s_texColor, m_textureColor);
						bgfx::setVertexBuffer(0, m_vbh);
						bgfx::setIndexBuffer(m_ibh);
						bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA);
						bgfx::submit(RENDER_PASS_GEOMETRY, m_geomProgram);
					}
				}

				float lightDirIntensity[4] = { lightDir.x, lightDir.y, lightDir.z, m_directionalIntensity };
				const float lightAmbient[4] = { m_enableIBL ? m_ambientIblIntensity : 0.0f, 0.0f, 0.0f, 0.0f };

				float lightMtx[16];
				const float sy = m_caps->originBottomLeft ? 0.5f : -0.5f;
				const float sz = 1.0f;
				const float mtxBias[16] =
				{
					0.5f, 0.0f, 0.0f, 0.0f,
					0.0f,   sy, 0.0f, 0.0f,
					0.0f, 0.0f,   sz, 0.0f,
					0.5f, 0.5f, 0.0f, 1.0f,
				};

				float mtxTmp[16];
				bx::mtxMul(mtxTmp, lightView, lightProj);
				bx::mtxMul(lightMtx, mtxTmp, mtxBias);

				float invViewProj[16];
				float viewProj[16];
				bx::mtxMul(viewProj, view, proj);
				bx::mtxInverse(invViewProj, viewProj);

				float shadowParams[4] = { m_shadowBias, m_shadowNormalBias, m_enableShadows ? 1.0f : 0.0f, 0.0f };

				bgfx::setUniform(u_lightDirIntensity, lightDirIntensity);
				bgfx::setUniform(u_lightAmbient, lightAmbient);
				bgfx::setUniform(u_lightMtx, lightMtx);
				bgfx::setUniform(u_shadowParams, shadowParams);
				bgfx::setUniform(u_invViewProj, invViewProj);
				bgfx::setTexture(0, s_normal, m_gbufferTex[1]);
				bgfx::setTexture(1, s_depth, m_gbufferTex[2]);
				bgfx::setTexture(2, s_shadowMap, m_shadowMapTex);
				screenSpaceQuad(m_caps->originBottomLeft);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
				bgfx::submit(RENDER_PASS_LIGHT, m_lightProgram);

				if (m_showShadowMap)
				{
					bgfx::setTexture(0, s_texColor, m_shadowMapTex);
					screenSpaceQuad(m_caps->originBottomLeft);
					bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
					bgfx::submit(RENDER_PASS_DEBUG, m_debugProgram);
				}
				else if (m_showGBuffer)
				{
					bgfx::setTexture(0, s_texColor, m_gbufferTex[0]);
					screenSpaceQuad(m_caps->originBottomLeft);
					bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
					bgfx::submit(RENDER_PASS_DEBUG, m_debugProgram);
				}
				else
				{
					bgfx::setTexture(0, s_albedo, m_gbufferTex[0]);
					bgfx::setTexture(1, s_light, m_lightBufferTex);
					screenSpaceQuad(m_caps->originBottomLeft);
					bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA);
					bgfx::submit(RENDER_PASS_COMBINE, m_combineProgram);
				}
			}
			else
			{
				bgfx::setViewRect(RENDER_PASS_GEOMETRY, 0, 0, uint16_t(m_width), uint16_t(m_height) );
				bgfx::touch(RENDER_PASS_GEOMETRY);
			}

			// Use debug font to print information about this example.
			bgfx::dbgTextClear();
			const bgfx::Stats* stats = bgfx::getStats();

			if (!m_supportedBackend)
			{
				bgfx::dbgTextPrintf(0, 1, 0x4f, "Deferred path fallback: backend not supported yet.");
				bgfx::dbgTextPrintf(0, 2, 0x0f, "Use OpenGL or Vulkan to enable deferred + shadow map + IBL.");
			}
			else if (!m_deferredSupported)
			{
				bgfx::dbgTextPrintf(0, 1, 0x4f, "Deferred path fallback: MRT/depth framebuffer requirements missing.");
			}
			else
			{
				bgfx::dbgTextPrintf(0, 1, 0x2f, "Deferred Step 4 active: directional shadow map.");
			}

			bgfx::dbgTextImage(
				  bx::max<uint16_t>(uint16_t(stats->textWidth/2), 20)-20
				, bx::max<uint16_t>(uint16_t(stats->textHeight/2),  6)-6
				, 40
				, 12
				, s_logo
				, 160
				);

			bgfx::dbgTextPrintf(0, 3, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters."
				, stats->width
				, stats->height
				, stats->textWidth
				, stats->textHeight
				);

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	entry::MouseState m_mouseState;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;

	// Deferred debug options.
	bool m_supportedBackend;
	bool m_deferredSupported;
	bool m_enableShadows;
	bool m_enableIBL;
	bool m_showGBuffer;
	bool m_showShadowMap;
	const bgfx::Caps* m_caps;

	bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh;
	bgfx::UniformHandle s_texColor;
	bgfx::UniformHandle s_albedo;
	bgfx::UniformHandle s_light;
	bgfx::UniformHandle s_normal;
	bgfx::UniformHandle s_depth;
	bgfx::UniformHandle s_shadowMap;
	bgfx::UniformHandle u_lightDirIntensity;
	bgfx::UniformHandle u_lightAmbient;
	bgfx::UniformHandle u_lightMtx;
	bgfx::UniformHandle u_shadowParams;
	bgfx::UniformHandle u_invViewProj;
	bgfx::ProgramHandle m_geomProgram;
	bgfx::ProgramHandle m_lightProgram;
	bgfx::ProgramHandle m_combineProgram;
	bgfx::ProgramHandle m_debugProgram;
	bgfx::ProgramHandle m_shadowProgram;
	bgfx::TextureHandle m_textureColor;
	bgfx::TextureHandle m_gbufferTex[3];
	bgfx::TextureHandle m_lightBufferTex;
	bgfx::TextureHandle m_shadowMapTex;
	bgfx::FrameBufferHandle m_gbuffer;
	bgfx::FrameBufferHandle m_lightBuffer;
	bgfx::FrameBufferHandle m_shadowMapFB;

	uint32_t m_oldWidth;
	uint32_t m_oldHeight;
	uint32_t m_oldReset;

	uint16_t m_shadowResolution;
	float m_shadowBias;
	float m_shadowNormalBias;
	float m_directionalIntensity;
	float m_ambientIblIntensity;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  Deferred
	, "00-deferred-pga"
	, "Deferred renderer (WIP): directional + ambient IBL + shadow map."
	, "https://bkaradzic.github.io/bgfx/examples.html#deferred-pga"
	);
