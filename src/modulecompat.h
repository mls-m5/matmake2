#pragma once

#if __has_feature(modules)

#define export_type export

#define export_fun export

#else

#define export_type

#define export_fun inline

#endif
