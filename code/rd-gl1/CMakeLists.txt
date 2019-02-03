#============================================================================
# Copyright (C) 2013 - 2015, OpenJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

if(BuildSPRdGL1)

	set(SPRDGL1RendererIncludeDirectories ${SPDir})
	set(SPRDGL1RendererIncludeDirectories
		${SharedDir}
		${SPRDGL1RendererIncludeDirectories}
		"${SPDir}/rd-gl1"
		"${GSLIncludeDirectory}"
		)

	# Modular Renderer crap (let's hope this works..)

	# Defines
	set(SPRDGL1Defines ${SharedDefines} "RENDERER" "_JK2EXE")

	# Files

	# GHOUL 2
	set(SPRDGL1G2Files
		"${SPDir}/ghoul2/G2.h"
		"${SPDir}/ghoul2/ghoul2_gore.h"
		"${SPDir}/rd-gl1/G2_API.cpp"
		"${SPDir}/rd-gl1/G2_bolts.cpp"
		"${SPDir}/rd-gl1/G2_bones.cpp"
		"${SPDir}/rd-gl1/G2_misc.cpp"
		"${SPDir}/rd-gl1/G2_surfaces.cpp"
		)
	source_group("ghoul2" FILES ${SPRDGL1G2Files})
	set(SPRDGL1Files ${SPRDGL1Files} ${SPRDGL1G2Files})

    # Transparently use either bundled or system jpeg.
    list(APPEND SPRDGL1RendererIncludeDirectories ${JPEG_INCLUDE_DIR})
    list(APPEND SPRDGL1RendererLibraries          ${JPEG_LIBRARIES})

    # Transparently use either bundled or system libpng.  Order is important --
    # libpng used zlib, so it must come before it on the linker command line.
    list(APPEND SPRDGL1RendererIncludeDirectories ${PNG_INCLUDE_DIRS})
    list(APPEND SPRDGL1RendererLibraries          ${PNG_LIBRARIES})

    # Transparently use either bundled or system zlib.
    list(APPEND SPRDGL1RendererIncludeDirectories ${ZLIB_INCLUDE_DIR})
    list(APPEND SPRDGL1RendererLibraries          ${ZLIB_LIBRARIES})

    # Transparently use our bundled minizip.
    list(APPEND SPRDGL1RendererIncludeDirectories ${MINIZIP_INCLUDE_DIRS})
    list(APPEND SPRDGL1RendererLibraries          ${MINIZIP_LIBRARIES})

    # Transparantly make use of all renderer directories
    list(APPEND SPRDGL1RendererIncludeDirectories "${SPDir}/rd-common")
    list(APPEND SPRDGL1RendererIncludeDirectories "${SPDir}/rd-gl1")

	# Misc files
	set(SPRDGL1CommonFiles
		"${SPDir}/qcommon/matcomp.cpp"
		"${SPDir}/qcommon/q_shared.cpp"
		"${SPDir}/qcommon/q_shared.h"
		"${SPDir}/qcommon/ojk_i_saved_game.h"
		"${SPDir}/qcommon/ojk_saved_game_class_archivers.h"
		"${SPDir}/qcommon/ojk_saved_game_helper.h"
		"${SPDir}/qcommon/ojk_saved_game_helper_fwd.h"
		"${SPDir}/qcommon/ojk_scope_guard.h"
		${SharedCommonFiles}
		)
	source_group("common" FILES ${SPRDGL1CommonFiles})
	set(SPRDGL1Files ${SPRDGL1Files} ${SPRDGL1CommonFiles})

	# Misc files
	set(SPRDGL1CommonSafeFiles
		${SharedCommonSafeFiles}
		)
	source_group("common/safe" FILES ${SPRDGL1CommonSafeFiles})
	set(SPRDGL1Files ${SPRDGL1Files} ${SPRDGL1CommonSafeFiles})

	# main files
	set(SPRDGL1MainFiles
		"${SPDir}/rd-gl1/glext.h"
		"${SPDir}/rd-gl1/qgl.h"
		"${SPDir}/rd-gl1/tr_arb.cpp"
		"${SPDir}/rd-gl1/tr_backend.cpp"
		"${SPDir}/rd-gl1/tr_bsp.cpp"
		"${SPDir}/rd-gl1/tr_cmds.cpp"
		"${SPDir}/rd-gl1/tr_curve.cpp"
		"${SPDir}/rd-gl1/tr_draw.cpp"
		"${SPDir}/rd-gl1/tr_ghoul2.cpp"
		"${SPDir}/rd-gl1/tr_image.cpp"
		"${SPDir}/rd-gl1/tr_init.cpp"
		"${SPDir}/rd-gl1/tr_light.cpp"
		"${SPDir}/rd-gl1/tr_local.h"
		"${SPDir}/rd-gl1/tr_main.cpp"
		"${SPDir}/rd-gl1/tr_marks.cpp"
		"${SPDir}/rd-gl1/tr_mesh.cpp"
		"${SPDir}/rd-gl1/tr_model.cpp"
		"${SPDir}/rd-gl1/tr_quicksprite.cpp"
		"${SPDir}/rd-gl1/tr_quicksprite.h"
		"${SPDir}/rd-gl1/tr_scene.cpp"
		"${SPDir}/rd-gl1/tr_shade.cpp"
		"${SPDir}/rd-gl1/tr_shade_calc.cpp"
		"${SPDir}/rd-gl1/tr_shader.cpp"
		"${SPDir}/rd-gl1/tr_shadows.cpp"
		"${SPDir}/rd-gl1/tr_skin.cpp"
		"${SPDir}/rd-gl1/tr_sky.cpp"
		"${SPDir}/rd-gl1/tr_stl.cpp"
		"${SPDir}/rd-gl1/tr_stl.h"
		"${SPDir}/rd-gl1/tr_subs.cpp"
		"${SPDir}/rd-gl1/tr_surface.cpp"
		"${SPDir}/rd-gl1/tr_surfacesprites.cpp"
		"${SPDir}/rd-gl1/tr_world.cpp"
		"${SPDir}/rd-gl1/tr_WorldEffects.cpp"
		"${SPDir}/rd-gl1/tr_WorldEffects.h"
		)
	source_group("renderer" FILES ${SPRDGL1MainFiles})
	set(SPRDGL1Files ${SPRDGL1Files} ${SPRDGL1MainFiles})

	set(SPRDGL1RdCommonFiles
		"${SPDir}/rd-common/mdx_format.h"
		"${SPDir}/rd-common/tr_common.h"
		"${SPDir}/rd-common/tr_font.cpp"
		"${SPDir}/rd-common/tr_font.h"
		"${SPDir}/rd-common/stb_image.h"
		"${SPDir}/rd-common/tr_image_load.cpp"
		"${SPDir}/rd-common/tr_image_stb.cpp"
		"${SPDir}/rd-common/tr_image_jpg.cpp"
		"${SPDir}/rd-common/tr_image_tga.cpp"
		"${SPDir}/rd-common/tr_image_png.cpp"
		"${SPDir}/rd-common/tr_noise.cpp"
		"${SPDir}/rd-common/tr_public.h"
		"${SPDir}/rd-common/tr_types.h")
	source_group("rd-common" FILES ${SPRDGL1RdCommonFiles})
	set(SPRDGL1Files ${SPRDGL1Files} ${SPRDGL1RdCommonFiles})

	find_package(OpenGL REQUIRED)
	set(SPRDGL1RendererIncludeDirectories ${SPRDGL1RendererIncludeDirectories} ${OPENGL_INCLUDE_DIR})
	set(SPRDGL1RendererLibraries ${SPRDGL1RendererLibraries} ${OPENGL_LIBRARIES})

	set(SPRDGL1RendererIncludeDirectories ${SPRDGL1RendererIncludeDirectories} ${OpenJKLibDir})

	function(add_sp_renderer_project ProjectName Label EngineName InstallDir Component)
		add_library(${ProjectName} SHARED ${SPRDGL1Files})
		if(NOT MSVC)
			# remove "lib" prefix for .so/.dylib files
			set_target_properties(${ProjectName} PROPERTIES PREFIX "")
		endif()

		if(WIN32)
			install(TARGETS ${ProjectName}
				RUNTIME
				DESTINATION ${InstallDir}
				COMPONENT ${Component})
		else(WIN32)
			if(MakeApplicationBundles AND (BuildSPEngine))
				install(TARGETS ${ProjectName}
					LIBRARY
					DESTINATION "${InstallDir}/${EngineName}.app/Contents/MacOS/"
					COMPONENT ${Component})
			else()
				install(TARGETS ${ProjectName}
					LIBRARY
					DESTINATION ${InstallDir}
					COMPONENT ${Component})
			endif()
		endif()

		set_target_properties(${ProjectName} PROPERTIES COMPILE_DEFINITIONS "${SPRDGL1Defines}")

		# Hide symbols not explicitly marked public.
		set_property(TARGET ${ProjectName} APPEND PROPERTY COMPILE_OPTIONS ${OPENJK_VISIBILITY_FLAGS})

		set_target_properties(${ProjectName} PROPERTIES INCLUDE_DIRECTORIES "${SPRDGL1RendererIncludeDirectories}")
		set_target_properties(${ProjectName} PROPERTIES PROJECT_LABEL ${Label})

		target_link_libraries(${ProjectName} ${SPRDGL1RendererLibraries})
	endfunction(add_sp_renderer_project)

	if(BuildSPRdGL1)
		add_sp_renderer_project(${SPRDGL1Renderer} "SP GL1 Renderer" ${SPEngine} ${JKAInstallDir} ${JKASPClientComponent})
	endif(BuildSPRdGL1)

endif(BuildSPRdGL1)
