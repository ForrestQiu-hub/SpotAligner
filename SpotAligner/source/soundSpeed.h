//------------------------------------------------------------------------
// Copyright(c) 2026 Forrest's Plug-ins.
//------------------------------------------------------------------------
// Speed of sound in air, used by the distance based alignment delay.
//
// The speed of sound is either entered by hand (m/s) or computed from the
// room conditions: temperature, relative humidity and static air pressure.
//
// The computation is the reference model for acoustical measurements:
//
//   O. Cramer, "The variation of the specific heat ratio and the speed of
//   sound in air with temperature, pressure, humidity, and CO2
//   concentration", J. Acoust. Soc. Am. 93 (5), 2510-2516, 1993. Eq. (15)
//
//   R. S. Davis, "Equation for the determination of the density of moist
//   air (1981/91)", Metrologia 29 (1), 67-70, 1992. Saturation vapour
//   pressure and enhancement factor.
//
// Reference values of this implementation:
//   dry air at 0 degC, 101.325 kPa        -> 331.45 m/s
//   20 degC, 50 % RH, 101.325 kPa         -> 344.02 m/s
// Cramer's fit is documented for 0 .. 30 degC; the alignment delay only
// needs the variation with temperature, so the model is evaluated over the
// full parameter range of the plug-in.
//------------------------------------------------------------------------
#pragma once

#include <cmath>

namespace MyCompanyName {
namespace SoundSpeed {

//------------------------------------------------------------------------
/** Standard air pressure in kPa (sea level). */
constexpr double kStandardPressureKPa = 101.325;

/** Molar fraction of CO2 in air: 0.04 % (the value Cramer uses). */
constexpr double kCo2MolarFraction = 0.0004;

/** Lowest speed of sound we expect indoors (cold, very humid air, m/s).
	Used to size the delay line for a given maximum distance. */
constexpr double kMinimumSpeedOfSound = 300.0;

//------------------------------------------------------------------------
/** Saturation vapour pressure of water in air, in Pa (Davis 1992).
	@param tKelvin absolute temperature in K */
inline double waterSaturationPressurePa (double tKelvin)
{
	// exp(A T^2 + B T + C + D / T)
	const double quadratic = 1.2378847e-5 * tKelvin * tKelvin + -1.9121316e-2 * tKelvin + 33.93711047;
	const double inverse = -6.3431645e3 / tKelvin;
	return std::exp (quadratic + inverse);
}

//------------------------------------------------------------------------
/** Enhancement factor of the saturation vapour pressure in air (Davis 1992).
	@param tKelvin absolute temperature in K
	@param pPa     static air pressure in Pa */
inline double enhancementFactor (double tKelvin, double pPa)
{
	return 1.00062 + 3.14e-8 * pPa + 5.6e-7 * tKelvin * tKelvin;
}

//------------------------------------------------------------------------
/** Molar fraction of water vapour in air for a given relative humidity.
	@param humidityPct relative humidity in percent (0 .. 100)
	@param tKelvin     absolute temperature in K
	@param pPa         static air pressure in Pa */
inline double waterMolarFraction (double humidityPct, double tKelvin, double pPa)
{
	const double fraction = humidityPct / 100.0;
	return fraction * enhancementFactor (tKelvin, pPa) * waterSaturationPressurePa (tKelvin) / pPa;
}

//------------------------------------------------------------------------
/** Speed of sound in moist air, in m/s.

	@param temperatureC air temperature in degrees Celsius
	@param humidityPct  relative humidity in percent (0 .. 100)
	@param pressureKPa  static air pressure in kPa
*/
inline double fromAirConditions (double temperatureC, double humidityPct, double pressureKPa)
{
	if (humidityPct < 0.0)
		humidityPct = 0.0;
	else if (humidityPct > 100.0)
		humidityPct = 100.0;

	if (pressureKPa < 1.0)
		pressureKPa = 1.0;

	double tKelvin = temperatureC + 273.15;
	if (tKelvin < 1.0)
		tKelvin = 1.0; // guard the 1/T term of the saturation pressure

	const double pPa = pressureKPa * 1000.0;
	const double xw = waterMolarFraction (humidityPct, tKelvin, pPa);
	const double xc = kCo2MolarFraction;

	// Cramer 1993, Eq. (15): temperature, water vapour, pressure and CO2
	// terms with their quadratic coefficients.
	const double t = temperatureC;
	const double t2 = t * t;

	double c = 331.5024 + 0.603055 * t + -0.000528 * t2;
	c += (51.471935 + 0.1495874 * t + -0.000782 * t2) * xw;
	c += (-1.82e-7 + 3.73e-8 * t + -2.93e-10 * t2) * pPa;
	c += (-85.20931 + -0.228525 * t + 5.91e-5 * t2) * xc;
	c += -2.835149 * xw * xw;
	c += -2.15e-13 * pPa * pPa;
	c += 29.179762 * xc * xc;
	c += 0.000486 * xw * pPa * xc;

	return c;
}

} // namespace SoundSpeed
} // namespace MyCompanyName
