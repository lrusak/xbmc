#version 100

uniform mat4 m_model;
uniform mat4 m_modelviewproj;
uniform mat3 m_normalMatrix;

attribute vec4 m_attrpos;
attribute vec3 m_normal;
attribute vec2 m_attrcord0;

vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);

varying vec4 vVaryingColor;
varying vec2 m_cord0;

void main()
{
  gl_Position = m_modelviewproj * m_attrpos;
  vec3 vEyeNormal = m_normalMatrix * m_normal;
  vec4 vPosition4 = m_model * m_attrpos;
  vec3 vPosition3 = vPosition4.xyz / vPosition4.w;
  vec3 vLightDir = normalize(lightSource.xyz - vPosition3);
  float diff = max(0.0, dot(vEyeNormal, vLightDir));
  vVaryingColor = vec4(diff * vec3(1.0, 1.0, 1.0), 1.0);
  m_cord0 = m_attrcord0;
}
