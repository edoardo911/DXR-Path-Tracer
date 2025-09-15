workspace "PathTracer"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release"
	}

	outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

	project "PathTracer"
		location "PathTracer"
		kind "WindowedApp"
		language "C++"

		targetdir ("bin/" .. outputdir .. "/%{prj.name}")
		objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

		files
		{
			"%{prj.name}/src/**.h",
			"%{prj.name}/src/**.cc",
			"%{prj.name}/src/**.cpp",
			"%{prj.name}/res/**.hlsl"
		}

		removefiles { "%{prj.name}/res/shaders/raytracing/**.hlsl" }
		removefiles { "%{prj.name}/res/shaders/raytracing" }

		filter "system:windows"
			cppdialect "C++20"
			staticruntime "On"
			systemversion "latest"

			includedirs
			{
				"vendor/dlss/include",
				"vendor/_NRD_SDK/Include",
				"vendor/tinygltf",
			}

			links
			{
				"NRD.lib",
			}

		filter "files:**.hlsl"
			removeflags "ExcludeFromBuild"
			shadermodel "5.1"
			shadertype "Compute"
			shaderobjectfileoutput "%{prj.location}/res/shaders/bin/%{file.basename}.cso"
		filter "files:**_vs.hlsl"
			shadertype "Vertex"
		filter "files:**_ps.hlsl"
			shadertype "Pixel"
		filter "files:**_gs.hlsl"
			shadertype "Geometry"

		filter "configurations:Debug"
			defines "UGE_DEBUG"
			symbols "On"
			buildoptions "/MDd"

			libdirs
			{
				"vendor/dlss/lib/debug/",
				"vendor/_NRD_SDK/Lib/Debug",
			}

			links
			{
				"nvsdk_ngx_d_dbg.lib",
			}

		filter "configurations:Release"
			defines "UGE_RELEASE"
			optimize "On"
			symbols "On"
			buildoptions "/MD /Ot"
			flags { "LinkTimeOptimization" }

			libdirs
			{
				"vendor/dlss/lib/release/",
				"vendor/_NRD_SDK/Lib/Release",
			}

			links
			{
				"nvsdk_ngx_d.lib",
			}