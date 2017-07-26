#pragma once

#include "Geometry.h"
#include "Utilities.h"

namespace rkg
{

	inline Vec3 HSVtoRGB(const Vec3& hsv) {

		// Branchless, taken from https://stackoverflow.com/a/19400360

		float hue_slice = 6.0 * hsv.x;                                            // In [0,6[
		float hue_slice_int = floor(hue_slice);
		float hue_slice_frac = hue_slice - hue_slice_int;                   // In [0,1[ for each hue slice

		Vec3  temp_rgb = Vec3(hsv.z * (1.0 - hsv.y),
			hsv.z * (1.0 - hsv.y * hue_slice_frac),
			hsv.z * (1.0 - hsv.y * (1.0 - hue_slice_frac)));

		// The idea here to avoid conditions is to notice that the conversion code can be rewritten:
		//    if      ( var_i == 0 ) { R = V         ; G = TempRGB.z ; B = TempRGB.x }
		//    else if ( var_i == 2 ) { R = TempRGB.x ; G = V         ; B = TempRGB.z }
		//    else if ( var_i == 4 ) { R = TempRGB.z ; G = TempRGB.x ; B = V     }
		// 
		//    else if ( var_i == 1 ) { R = TempRGB.y ; G = V         ; B = TempRGB.x }
		//    else if ( var_i == 3 ) { R = TempRGB.x ; G = TempRGB.y ; B = V     }
		//    else if ( var_i == 5 ) { R = V         ; G = TempRGB.x ; B = TempRGB.y }
		//
		// This shows several things:
		//  . A separation between even and odd slices
		//  . If slices (0,2,4) and (1,3,5) can be rewritten as basically being slices (0,1,2) then
		//      the operation simply amounts to performing a "rotate right" on the RGB components
		//  . The base value to rotate is either (V, B, R) for even slices or (G, V, R) for odd slices

		float is_odd_slice = fmod(hue_slice_int, 2.0);                          // 0 if even (slices 0, 2, 4), 1 if odd (slices 1, 3, 5)
		float three_slice_selector = 0.5 * (hue_slice_int - is_odd_slice);          // (0, 1, 2) corresponding to slices (0, 2, 4) and (1, 3, 5)

		Vec3 scrolling_rgb_for_even_slices = Vec3(hsv.z, temp_rgb.z, temp_rgb.x);           // (V, Temp Blue, Temp Red) for even slices (0, 2, 4)
		Vec3 scrolling_rgb_for_odd_slices = Vec3(temp_rgb.y, hsv.z, temp_rgb.x);  // (Temp Green, V, Temp Red) for odd slices (1, 3, 5)
		Vec3 scrolling_rgb = Lerp(scrolling_rgb_for_even_slices, scrolling_rgb_for_odd_slices, is_odd_slice);

		float is_not_first_slice = Clamp(three_slice_selector, 0.f, 1.f);                   // 1 if NOT the first slice (true for slices 1 and 2)
		float is_not_second_slice = Clamp(three_slice_selector - 1.0, 0.f, 1.f);              // 1 if NOT the first or second slice (true only for slice 2)

		return Lerp(scrolling_rgb, 
					Lerp(Vec3(scrolling_rgb.z, scrolling_rgb.x, scrolling_rgb.y), 
						Vec3(scrolling_rgb.y, scrolling_rgb.z, scrolling_rgb.x), 
						is_not_second_slice), 
					is_not_first_slice);    // Make the RGB rotate right depending on final slice index
	}

}