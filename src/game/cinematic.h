#pragma once
#include <d3d11.h>

// Cinemáticas a pantalla completa. Con la pantalla dividida, Halo CE dibuja las cinemáticas una sola vez sobre toda
// la pantalla: mientras duran, cada amigo recibe la pantalla entera reducida a su tamaño en vez de su cuarto.
//
// La marca "cinemática en curso" es un byte de halo1.dll (+0x1B87A20: 1 durante la cinemática, 0 en el juego;
// +0x1B87A21 vale lo contrario), encontrado comparando la memoria con y sin cinemática. Solo halo1.dll de MCC Steam
// 1.3528: se comprueba la marca de tiempo del módulo.
bool cinematic_now();

// La pantalla entera reducida a width x height (nullptr si no se pudo). La textura es del módulo: no liberarla.
ID3D11Texture2D* cinematic_frame(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* back_buffer,
                                 UINT width, UINT height);
