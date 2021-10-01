#version 100

precision mediump float;

uniform sampler2D m_samp0;

varying vec2 m_cord0;
varying vec4 vVaryingColor;

void main()
{
  gl_FragColor = vVaryingColor * texture2D(m_samp0, m_cord0);
}
