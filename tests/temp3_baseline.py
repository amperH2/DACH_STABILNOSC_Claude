"""Normalize only the explicitly tested TEMP3 policy integration for older invariants."""
def before_policy(code):
 return (code
  .replace('cfgApplyHysteresis(); weatherPolicyLoad(); return;','cfgApplyHysteresis(); return;')
  .replace('  weatherPolicyLoad();\n','')
  .replace('  // Weather-policy choices are retained by a threshold reset.','  g_cloudCanTriggerUnsafe = (CLOUD_CAN_TRIGGER_UNSAFE != 0);')
  .replace('      policySkyReady() && policyWindReady() && !policyMoistureBlocked() && !g_weatherPolicyFault;', '      (g_mlxSamples[0].usable(now) || g_mlxSamples[1].usable(now)) &&\n      (!ANEMOMETER_ENABLED || windMeasurementUsable());')
  .replace('    (g_weatherPolicy.clouds && g_cloudUnsafeLatched) || uint32_t(millis()-lastSensorsRead)>3000UL ||\n    !policySkyReady() || !policyWindReady() ||', '    g_cloudUnsafeLatched || uint32_t(millis()-lastSensorsRead)>3000UL ||\n    (MLX_FAIL_UNSAFE && !g_mlxSamples[0].usable(millis()) && !g_mlxSamples[1].usable(millis())) ||\n    (ANEMOMETER_ENABLED && !windMeasurementUsable()) ||')
  .replace('g_weatherPolicy.rain && g_rain && !g_alertRainSent','g_rain && !g_alertRainSent')
  .replace('g_weatherPolicy.wind && g_anemometerEnabled','g_anemometerEnabled')
  .replace('  const uint8_t hardMask=((g_weatherPolicy.rain&&g_rain)?1:0)|((g_weatherPolicy.wind&&g_windUnsafeLatched)?2:0)|((g_weatherPolicy.clouds&&g_denseCloud.latched)?4:0);','  const uint8_t hardMask=(g_rain?1:0)|(g_windUnsafeLatched?2:0)|(g_denseCloud.latched?4:0);')
  .replace('((g_weatherPolicy.rain&&g_rain)?String("DESZCZ"):(g_weatherPolicy.wind&&g_windUnsafeLatched)?String("WIATR"):g_reason)','(g_rain?String("DESZCZ"):g_windUnsafeLatched?String("WIATR"):g_reason)'))
