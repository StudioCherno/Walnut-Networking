project "Walnut-Networking"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "Source/**.h", "Source/**.cpp" }

   includedirs
   {
      "Source",

      "vendor/GameNetworkingSockets/include",

      --------------------------------------------------------
      -- Walnut includes
      -- Assumes we are in Walnut-Modules/Walnut-Networking
      "../../Walnut/Source",

      "../../vendor/imgui",
      "../../vendor/glfw/include",
      "../../vendor/glm",
      "../../vendor/spdlog/include",
      --------------------------------------------------------
   }

   targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }
      files { "Platform/Windows/**.h", "Platform/Windows/**.cpp" }
      includedirs { "Platform/Windows" }
      links { "Ws2_32.lib" }
      buildoptions { "/utf-8" }

   filter "system:linux"
      defines { "WL_PLATFORM_LINUX" }
      files { "Platform/Linux/**.h", "Platform/Linux/**.cpp" }
      includedirs { "Platform/Linux" }

  filter { "system:windows", "configurations:Debug" }	
      links
      {
          "vendor/GameNetworkingSockets/bin/Windows/Debug/GameNetworkingSockets.lib"
      }

  filter { "system:windows", "configurations:Release or configurations:Dist" }	
      links
      {
          "vendor/GameNetworkingSockets/bin/Windows/Release/GameNetworkingSockets.lib"
      }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"