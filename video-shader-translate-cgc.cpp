#include "io.h"

#ifdef HAVE_CG_SHADERS
#include <Cg/cg.h>

namespace {

  //CG_PROFILE_GLSLV   = 7007, /* GLSL vertex shader                                       */
  //CG_PROFILE_GLSLF   = 7008, /* GLSL fragment shader                                     */
  //CG_PROFILE_GLSLG   = 7016, /* GLSL geometry shader                                     */
  //CG_PROFILE_GLSLC   = 7009, /* Combined GLSL program                                    */
}

char * video::shader::translate_cgc(lang_t from, lang_t to, const char * text)
{
	if (from!=la_cg) return NULL;
	if (to!=la_glsl) return NULL;
	
#define e printf("e=%s\n",cgGetLastListing(context));
	CGcontext context = cgCreateContext();
	CGprogram vertex = cgCreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLV, "main_vertex", NULL);
	CGprogram fragment = cgCreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLF, "main_fragment", NULL);
	cgCompileProgram(vertex);
	cgCompileProgram(fragment);
	if (!cgIsProgramCompiled(vertex)) goto error;
	if (!cgIsProgramCompiled(fragment)) goto error;
	
	//TODO: do this properly
	printf("%s\n", cgGetProgramString(vertex, CG_COMPILED_PROGRAM));
	printf("%s\n", cgGetProgramString(fragment, CG_COMPILED_PROGRAM));
	goto error;
	
	return NULL;
error:
	cgDestroyContext(context);
	return NULL;
}
#endif
