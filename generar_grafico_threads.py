#!/usr/bin/env python3
"""
Generador de Gráfico de Rendimiento de Threads
===============================================
Lee un archivo de log de benchmark y genera un gráfico de barras
mostrando el tiempo de ejecución vs cantidad de threads.
"""

import sys
import os
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def generar_grafico(log_file):
    """Genera gráfico de barras: threads vs tiempo."""
    
    if not os.path.exists(log_file):
        print(f"❌ Error: No se encontró el archivo {log_file}")
        return False
    
    # Leer datos
    df = pd.read_csv(log_file)
    
    if df.empty:
        print(f"❌ Error: El archivo de log está vacío")
        return False
    
    # Filtrar errores (-1 indica error en ejecución)
    df_valid = df[df['tiempo_ms'] > 0].copy()
    
    if df_valid.empty:
        print(f"❌ Error: No hay datos válidos en el log")
        return False
    
    # Ordenar por cantidad de threads (menor a mayor)
    df_valid = df_valid.sort_values('threads').reset_index(drop=True)
    
    # Determinar si usar milisegundos o segundos
    max_time_sec = df_valid['tiempo_sec'].max()
    use_seconds = max_time_sec > 5  # Si tarda más de 5 seg, usar segundos
    
    # Crear gráfico
    fig, ax = plt.subplots(figsize=(12, 7), facecolor='white')
    
    if use_seconds:
        y_data = df_valid['tiempo_sec']
        y_label = 'Tiempo de Ejecución (segundos)'
    else:
        y_data = df_valid['tiempo_ms']
        y_label = 'Tiempo de Ejecución (milisegundos)'
    
    threads = df_valid['threads']
    
    # Crear barras con colores del gradiente viridis
    colors = plt.cm.viridis([i / (len(threads)-1) if len(threads) > 1 else 0.5 for i in range(len(threads))])
    bars = ax.bar(range(len(threads)), y_data, color=colors, edgecolor='white', linewidth=2, width=0.7)
    
    # Añadir valores encima de las barras
    for bar, val in zip(bars, y_data):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{val:.2f}' if use_seconds else f'{int(val)}',
                ha='center', va='bottom', fontsize=11, fontweight='bold')
    
    ax.set_xlabel('Cantidad de Threads', fontsize=14, fontweight='bold')
    ax.set_ylabel(y_label, fontsize=14, fontweight='bold')
    ax.set_title('Análisis de Rendimiento: Tiempo vs Threads', 
                 fontsize=16, fontweight='bold', pad=20)
    
    # Configurar eje X con los valores reales de threads
    ax.set_xticks(range(len(threads)))
    ax.set_xticklabels([str(t) for t in threads])
    
    # Grid
    ax.grid(True, alpha=0.3, axis='y', linestyle='--')
    ax.set_axisbelow(True)
    
    plt.tight_layout()
    
    # Guardar en graficos/Benchmark/latest/
    # Extraer el nombre base del log_file y cambiar extensión
    log_basename = os.path.basename(log_file)
    output_filename = log_basename.replace('.log', '.png')
    output_file = os.path.join('graficos', 'Benchmark', 'latest', output_filename)
    
    # Crear directorio si no existe
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    plt.savefig(output_file, dpi=150, bbox_inches='tight', facecolor='white')
    plt.close()
    
    print(f"✅ Gráfico generado: {output_file}")
    return True


def main():
    if len(sys.argv) != 2:
        print("Uso: python3 generar_grafico_threads.py <archivo_log>")
        sys.exit(1)
    
    log_file = sys.argv[1]
    
    if not generar_grafico(log_file):
        sys.exit(1)


if __name__ == '__main__':
    main()
