#include "io.h"
#include "os.h"

#ifdef HAVE_CG_SHADERS
#define CG_EXPLICIT // disable prototypes
#include <Cg/cg.h>

char * video::shader::translate_cgc(lang_t from, lang_t to, const char * text)
{
	if (from!=la_cg) return NULL;
	if (to!=la_glsl) return NULL;
	
#if 0
#define CG_SYM(ret, name, args) CG_SYM_N("cg"#name, ret, name, args)
#define CG_SYMS() \
	CG_SYM(CGcontext, CreateContext, (void)) \
	
#define CG_SYM_N(str, ret, name, args) ret (*name) args;
	struct { CG_SYMS()  } cg;
#undef CG_SYM_N
#define CG_SYM_N(str, ret, name, args) str,
	static const char * const cg_names[]={ CG_SYMS() };
#undef CG_SYM_N
	
	
	CGcontext context = cg.CreateContext();
#define e printf("e=%s\n",cgGetLastListing(context));
	//TODO: process with random profile with flags "-E", "-D", "PARAMETER_UNIFORM", "-I", (dir), NULL to get #includes out of the way
	
//def preprocess_vertex(source_data):
//   input_data = source_data.split('\n')
//   ret = []
//   for line in input_data:
//      if ('uniform' in line) and (('float4x4' in line) or ('half4x4' in line)):
//         ret.append('#pragma pack_matrix(column_major)\n')
//         ret.append(line)
//         ret.append('#pragma pack_matrix(row_major)\n')
//      else:
//         ret.append(line)
//   return '\n'.join(ret)
	
	const char * args[]={ "-D", "PARAMETER_UNIFORM", NULL };
	CGprogram vertex = cgCreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLV, "main_vertex", args);
	CGprogram fragment = cgCreateProgram(context, CG_SOURCE, text, CG_PROFILE_GLSLF, "main_fragment", args);
	cgCompileProgram(vertex);
	cgCompileProgram(fragment);
	if (!cgIsProgramCompiled(vertex)) goto error;
	if (!cgIsProgramCompiled(fragment)) goto error;
	
	//TODO: do this properly
	printf("%s\n", cgGetProgramString(vertex, CG_COMPILED_PROGRAM));
	printf("%s\n", cgGetProgramString(fragment, CG_COMPILED_PROGRAM));
	goto error;
	
	cgDestroyContext(context);
return NULL;
error:
	cgDestroyContext(context);
#endif
	
	return NULL;
}
#endif
