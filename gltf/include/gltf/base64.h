#ifndef _GLTF_BASE64_H_
#define _GLTF_BASE64_H_

/*
base64.cpp and base64.h

base64 encoding and decoding with C++.

Version: 1.01.00

Copyright (C) 2004-2017 Ren� Nyffenegger

This source code is provided 'as-is', without any express or implied
warranty. In no event will the author be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this source code must not be misrepresented; you must not
claim that you wrote the original source code. If you use this source code
in a product, an acknowledgment in the product documentation would be
appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original source code.

3. This notice may not be removed or altered from any source distribution.

Ren� Nyffenegger rene.nyffenegger@adp-gmbh.ch

*/

#include <string>
#include <string_view>

namespace base64 {
	std::string encode(const std::string_view &string);
	std::string decode(const std::string_view &encoded_string);
} // namespace base64

#endif // _GLTF_BASE64_H_
