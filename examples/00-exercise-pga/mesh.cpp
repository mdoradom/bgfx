/*
 * Copyright 2011-2026 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"

namespace
{

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
			.add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
	}

	static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosTexCoord0Vertex::ms_layout;

void screenSpaceQuad(bool _originBottomLeft)
{
	if (3 == bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_layout) )
	{
		bgfx::TransientVertexBuffer vb = {};
		bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_layout);
		auto* vertex = reinterpret_cast<PosTexCoord0Vertex*>(vb.data);

		float minv = 0.0f;
		float maxv = 2.0f;
		if (_originBottomLeft)
		{
			const float temp = minv;
			minv = maxv;
			maxv = temp;
			minv -= 1.0f;
			maxv -= 1.0f;
		}

		vertex[0] = { -1.0f, 0.0f, 0.0f, -1.0f, minv };
		vertex[1] = {  1.0f, 0.0f, 0.0f,  1.0f, minv };
		vertex[2] = {  1.0f, 2.0f, 0.0f,  1.0f, maxv };

		bgfx::setVertexBuffer(0, &vb);
	}
}

constexpr bgfx::ViewId kViewScene = 0;
constexpr bgfx::ViewId kViewPost  = 1;

class ExampleMesh : public entry::AppI
{
public:
	ExampleMesh(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_width(0)
		, m_height(0)
		, m_debug(BGFX_DEBUG_NONE)
		, m_reset(BGFX_RESET_NONE)
		, m_mesh(nullptr)
		, m_program(BGFX_INVALID_HANDLE)
		, u_time(BGFX_INVALID_HANDLE)
		, m_filterCoverage(0.0f)
		, m_rtWidth(0)
		, m_rtHeight(0)
		, m_sceneFb(BGFX_INVALID_HANDLE)
		, m_postProgram(BGFX_INVALID_HANDLE)
		, u_filter(BGFX_INVALID_HANDLE)
		, s_sceneColor(BGFX_INVALID_HANDLE)
	{
	}

	void recreateFrameBuffer()
	{
		if (bgfx::isValid(m_sceneFb) )
		{
			bgfx::destroy(m_sceneFb);
		}

		const uint64_t colorFlags = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		const uint64_t depthFlags = BGFX_TEXTURE_RT_WRITE_ONLY;
		bgfx::TextureHandle fbTex[] = {
			bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8, colorFlags),
			bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::D24S8, depthFlags),
		};
		m_sceneFb = bgfx::createFrameBuffer(BX_COUNTOF(fbTex), fbTex, true);
		m_rtWidth = m_width;
		m_rtHeight = m_height;
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_NONE;
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

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
				, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
				, 0x303030ff
				, 1.0f
				, 0
				);

		u_time = bgfx::createUniform("u_time", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4);
		u_filter = bgfx::createUniform("u_filter", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4);
		s_sceneColor = bgfx::createUniform("s_sceneColor", bgfx::UniformType::Sampler);
		PosTexCoord0Vertex::init();

		// Create program from shaders.
		m_program = loadProgram("vs_mesh", "fs_mesh");
		m_postProgram = loadProgram("vs_postprocess", "fs_postprocess_bw");

		recreateFrameBuffer();

		m_mesh = meshLoad("meshes/bunny.bin");

		imguiCreate();

		m_frameTime.reset();
	}

	int shutdown() override
	{
		imguiDestroy();

		meshUnload(m_mesh);

		// Cleanup.
		bgfx::destroy(m_program);
		bgfx::destroy(m_postProgram);

		bgfx::destroy(u_time);
		bgfx::destroy(u_filter);
		bgfx::destroy(s_sceneColor);
		bgfx::destroy(m_sceneFb);

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			if (0 == m_width || 0 == m_height)
			{
				return true;
			}

			if (m_rtWidth != m_width || m_rtHeight != m_height)
			{
				recreateFrameBuffer();
			}

			m_frameTime.frame();
			auto time = bx::toSeconds<float>(m_frameTime.getDurationTime() );
			bgfx::setFrameUniform(u_time, &time);
			const float filterParams[4] = { m_filterCoverage, 0.01f, 0.0f, 0.0f };
			bgfx::setFrameUniform(u_filter, filterParams);

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
			ImGui::SetNextWindowPos(ImVec2(20.0f, 100.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(360.0f, 120.0f), ImGuiCond_FirstUseEver);
			ImGui::Begin("Black n White filter", nullptr, 0);
			ImGui::SliderFloat("Coverage", &m_filterCoverage, 0.0f, 1.0f);
			ImGui::End();

			imguiEndFrame();

			const bgfx::Caps* caps = bgfx::getCaps();

			const bx::Vec3 at  = { 0.0f, 1.0f,  0.0f };
			const bx::Vec3 eye = { 0.0f, 1.0f, -2.5f };

			// Set view and projection matrix for view 0.
			{
				float view[16];
				bx::mtxLookAt(view, eye, at);

				float proj[16];
				bx::mtxProj(proj, 60.0f, float(m_width)/float(m_height), 0.1f, 100.0f, caps->homogeneousDepth);
				bgfx::setViewTransform(kViewScene, view, proj);
				bgfx::setViewRect(kViewScene, 0, 0, uint16_t(m_width), uint16_t(m_height) );
				bgfx::setViewFrameBuffer(kViewScene, m_sceneFb);
				bgfx::setViewClear(kViewScene
					, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
					, 0x303030ff
					, 1.0f
					, 0
					);
			}

			float mtx[16];
			bx::mtxRotateXY(mtx
				, 0.0f
				, time*0.37f
				);

			meshSubmit(m_mesh, kViewScene, m_program, mtx);

			float ortho[16];
			bx::mtxOrtho(ortho, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, caps->homogeneousDepth);
			bgfx::setViewTransform(kViewPost, nullptr, ortho);
			bgfx::setViewRect(kViewPost, 0, 0, uint16_t(m_width), uint16_t(m_height) );
			bgfx::setViewFrameBuffer(kViewPost, BGFX_INVALID_HANDLE);
			bgfx::setViewClear(kViewPost, BGFX_CLEAR_COLOR, 0x303030ff, 1.0f, 0);
			bgfx::setTexture(0, s_sceneColor, bgfx::getTexture(m_sceneFb, 0) );
			bgfx::setState(BGFX_STATE_WRITE_RGB|BGFX_STATE_WRITE_A);
			screenSpaceQuad(caps->originBottomLeft);
			bgfx::submit(kViewPost, m_postProgram);

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
	uint32_t m_rtWidth;
	uint32_t m_rtHeight;

	FrameTime m_frameTime;
	float m_filterCoverage;

	Mesh* m_mesh;
	bgfx::ProgramHandle m_program;
	bgfx::ProgramHandle m_postProgram;
	bgfx::FrameBufferHandle m_sceneFb;
	bgfx::UniformHandle u_time;
	bgfx::UniformHandle u_filter;
	bgfx::UniformHandle s_sceneColor;
};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExampleMesh
	, "04-mesh"
	, "Loading meshes."
	, "https://bkaradzic.github.io/bgfx/examples.html#mesh"
	);
