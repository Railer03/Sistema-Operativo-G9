#!/usr/bin/env python3
"""
Generador de Gráficos Estadísticos para Pong Multiplayer
=========================================================
Lee el archivo de logs (pong_stats.log) y genera 4 imágenes:

1. grafico_goles.png      - Pie chart: distribución de goles
2. grafico_partidos.png   - Scatter: tiempo para marcar cada gol
3. grafico_velocidad.png  - Línea: velocidad durante el partido
4. grafico_colisiones.png - Barras: colisiones de pelota por equipo
"""

import os
import sys
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

COLORS = {'Red': '#E74C3C', 'Blue': '#3498DB'}


def load_log(log_file_path):
    """Carga y separa el log por tipo de evento."""
    
    if not os.path.exists(log_file_path):
        print(f"❌ No se encontró: {log_file_path}")
        return None, None, None, None
    
    df = pd.read_csv(log_file_path)
    if df.empty:
        print("❌ Log vacío")
        return None, None, None, None
    
    goals = df[df['event_type'] == 'GOAL'].copy()
    matches = df[df['event_type'] == 'MATCH'].copy()
    collisions = df[df['event_type'] == 'COLLISION'].copy()
    speeds = df[df['event_type'] == 'SPEED'].copy()
    
    if not goals.empty:
        goals.columns = ['event_type', 'timestamp', 'epoch_sec', 'match_id', 
                        'team', 'team_name', 'goal_number', 'time_since_last_goal', 
                        'match_time', '_1']
        goals = goals.drop(columns=['event_type', '_1'])
    
    if not matches.empty:
        matches.columns = ['event_type', 'timestamp', 'epoch_sec', 'match_id',
                          'winner_team', 'winner_name', 'duration_sec', 
                          'final_score_red', 'final_score_blue', '_1']
        matches = matches.drop(columns=['event_type', '_1'])
    
    if not collisions.empty:
        collisions.columns = ['event_type', 'timestamp', 'epoch_sec', 'match_id',
                             'team', 'team_name', 'slot', 'collision_count', '_1', '_2']
        collisions = collisions.drop(columns=['event_type', '_1', '_2'])
    
    if not speeds.empty:
        speeds.columns = ['event_type', 'timestamp', 'epoch_sec', 'match_id',
                         'speed_px_per_sec', 'velocity_x', 'velocity_y', '_1', '_2', '_3']
        speeds = speeds.drop(columns=['event_type', '_1', '_2', '_3'])
    
    return goals, matches, collisions, speeds


def grafico_1_goles(df, output_path):
    """Pie chart: distribución de goles por equipo."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor='white')
    
    if df is None or df.empty:
        ax.text(0.5, 0.5, 'Sin datos', ha='center', va='center', fontsize=20)
        ax.axis('off')
    else:
        red = len(df[df['team_name'] == 'Red'])
        blue = len(df[df['team_name'] == 'Blue'])
        total = red + blue
        
        sizes = [red, blue]
        labels = [f'Red\n{red} goles', f'Blue\n{blue} goles']
        colors = [COLORS['Red'], COLORS['Blue']]
        explode = (0.05, 0.05)
        
        wedges, texts, autotexts = ax.pie(
            sizes,
            labels=labels,
            colors=colors,
            autopct='%1.1f%%',
            startangle=90,
            explode=explode,
            shadow=True,
            textprops={'fontsize': 14, 'weight': 'bold'}
        )
        
        for autotext in autotexts:
            autotext.set_color('white')
            autotext.set_fontsize(16)
            autotext.set_weight('bold')
        
        ax.set_title(f'Distribución de Goles\nTotal: {total} goles', 
                    fontsize=18, weight='bold', pad=20)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
    plt.close()
    print(f"   ✓ {os.path.basename(output_path)}")


def grafico_2_tiempo_goles(df, output_path):
    """Scatter: tiempo para marcar cada gol."""
    fig, ax = plt.subplots(figsize=(12, 7), facecolor='white')
    
    if df is None or df.empty:
        ax.text(0.5, 0.5, 'Sin datos', ha='center', va='center', fontsize=20)
        ax.axis('off')
    else:
        df = df.sort_values('epoch_sec').reset_index(drop=True)
        df['time_since_last_goal'] = pd.to_numeric(df['time_since_last_goal'], errors='coerce')
        df['goal_number'] = pd.to_numeric(df['goal_number'], errors='coerce')
        
        # Detectar empates
        df['time_rounded'] = df['time_since_last_goal'].round(1)
        plotted = set()
        
        for idx, row in df.iterrows():
            gnum = row['goal_number']
            time_val = row['time_since_last_goal']
            team = row['team_name']
            
            # Verificar empate
            same = df[(df['goal_number'] == gnum) & (df.index != idx)]
            is_tie = False
            for _, other in same.iterrows():
                if abs(other['time_since_last_goal'] - time_val) < 0.5:
                    is_tie = True
                    break
            
            key = (gnum, round(time_val, 1))
            if is_tie and key not in plotted:
                ax.scatter(gnum, time_val, c='#9B59B6', s=150, alpha=0.9, 
                          edgecolors='white', linewidth=2, zorder=5)
                plotted.add(key)
            elif not is_tie:
                color = COLORS['Red'] if team == 'Red' else COLORS['Blue']
                ax.scatter(gnum, time_val, c=color, s=120, alpha=0.8, 
                          edgecolors='white', linewidth=1.5)
        
        # Leyenda
        from matplotlib.lines import Line2D
        legend = [
            Line2D([0], [0], marker='o', color='w', markerfacecolor=COLORS['Red'], 
                   markersize=12, label='Red'),
            Line2D([0], [0], marker='o', color='w', markerfacecolor=COLORS['Blue'], 
                   markersize=12, label='Blue'),
            Line2D([0], [0], marker='o', color='w', markerfacecolor='#9B59B6', 
                   markersize=12, label='Both teams')
        ]
        ax.legend(handles=legend, loc='upper right', fontsize=11)
        
        ax.set_xlabel('Número de Gol', fontsize=13, weight='bold')
        ax.set_ylabel('Tiempo para Marcar (segundos)', fontsize=13, weight='bold')
        ax.set_title('Tiempo para Marcar Cada Gol', fontsize=16, weight='bold')
        
        # Escala Y: múltiplos de 10, techo redondeado
        max_time = df['time_since_last_goal'].max()
        y_max = int(((max_time // 10) + 1) * 10)
        ax.set_yticks(range(0, y_max + 1, 10))
        ax.set_ylim(0, y_max)
        
        max_goal = int(df['goal_number'].max())
        ax.set_xticks(range(1, max_goal + 1))
        ax.set_xlim(0.5, max_goal + 0.5)
        ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
    plt.close()
    print(f"   ✓ {os.path.basename(output_path)}")


def grafico_3_velocidad(df, output_path):
    """Línea: velocidad de la pelota durante el partido."""
    fig, ax = plt.subplots(figsize=(12, 7), facecolor='white')
    
    if df is None or df.empty:
        ax.text(0.5, 0.5, 'Sin datos', ha='center', va='center', fontsize=20)
        ax.axis('off')
    else:
        df = df.sort_values('epoch_sec').reset_index(drop=True)
        df['speed_px_per_sec'] = pd.to_numeric(df['speed_px_per_sec'], errors='coerce')
        df['epoch_sec'] = pd.to_numeric(df['epoch_sec'], errors='coerce')
        
        start = df['epoch_sec'].min()
        end = df['epoch_sec'].max()
        duration = end - start
        df['time_rel'] = df['epoch_sec'] - start
        
        ax.plot(df['time_rel'], df['speed_px_per_sec'], 
               color='#9B59B6', linewidth=2.5, label='Velocidad')
        ax.fill_between(df['time_rel'], df['speed_px_per_sec'], 
                        alpha=0.2, color='#9B59B6')
        
        avg = df['speed_px_per_sec'].mean()
        ax.axhline(y=avg, color='orange', linestyle='--', linewidth=2, 
                  alpha=0.7, label=f'Promedio: {avg:.0f} px/s')
        
        ax.set_xlabel('Tiempo de Partido', fontsize=13, weight='bold')
        ax.set_ylabel('Velocidad (px/s)', fontsize=13, weight='bold')
        ax.set_title('Velocidad de la Pelota Durante el Partido', 
                    fontsize=16, weight='bold')
        
        # 5 marcas en el eje X
        ticks = [0, duration * 0.25, duration * 0.5, duration * 0.75, duration]
        labels = ['Inicio', '25%', '50%', '75%', 'Fin']
        ax.set_xticks(ticks)
        ax.set_xticklabels(labels)
        ax.set_xlim(0, duration)
        
        ax.legend(loc='upper left', fontsize=11)
        ax.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
    plt.close()
    print(f"   ✓ {os.path.basename(output_path)}")


def grafico_4_colisiones(df, output_path):
    """Barras: colisiones de pelota por equipo."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor='white')
    
    if df is None or df.empty:
        ax.text(0.5, 0.5, 'Sin datos', ha='center', va='center', fontsize=20)
        ax.axis('off')
    else:
        df['collision_count'] = pd.to_numeric(df['collision_count'], errors='coerce')
        team_col = df.groupby('team_name')['collision_count'].sum()
        
        teams = ['Red', 'Blue']
        counts = [team_col.get('Red', 0), team_col.get('Blue', 0)]
        colors = [COLORS['Red'], COLORS['Blue']]
        
        bars = ax.bar(teams, counts, color=colors, edgecolor='white', 
                     linewidth=3, width=0.6)
        
        ax.set_xlabel('Equipo', fontsize=13, weight='bold')
        ax.set_ylabel('Total de Colisiones', fontsize=13, weight='bold')
        ax.set_title('Colisiones de Pelota por Equipo', fontsize=16, weight='bold')
        ax.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches='tight', facecolor='white')
    plt.close()
    print(f"   ✓ {os.path.basename(output_path)}")


def main():
    if len(sys.argv) < 3:
        print("Uso: python3 generate_stats.py <log_file> <output_dir>")
        print("  log_file: ruta al archivo .log")
        print("  output_dir: directorio donde guardar los gráficos")
        sys.exit(1)
    
    log_file = sys.argv[1]
    output_dir = sys.argv[2]
    
    log_file = os.path.abspath(log_file)
    output_dir = os.path.abspath(output_dir)
    
    if not os.path.exists(log_file):
        print(f"❌ No existe: {log_file}")
        sys.exit(1)
    
    # Crear directorio de salida si no existe
    os.makedirs(output_dir, exist_ok=True)
    
    print(f"\n{'='*50}")
    print(f"  Generador de Estadísticas de Pong")
    print(f"{'='*50}")
    print(f"Log: {log_file}")
    print(f"Gráficos en: {output_dir}\n")
    
    goals, matches, collisions, speeds = load_log(log_file)
    
    if all(d is None or d.empty for d in [goals, matches, collisions, speeds]):
        print("\n❌ No hay datos en el log.\n")
        sys.exit(1)
    
    print(f"Datos encontrados:")
    print(f"   • Goles: {len(goals) if goals is not None else 0}")
    print(f"   • Partidos: {len(matches) if matches is not None else 0}")
    print(f"   • Colisiones: {len(collisions) if collisions is not None else 0}")
    print(f"   • Velocidad: {len(speeds) if speeds is not None else 0}")
    print(f"\nGenerando gráficos...")
    
    grafico_1_goles(goals, os.path.join(output_dir, 'grafico_goles.png'))
    grafico_2_tiempo_goles(goals, os.path.join(output_dir, 'grafico_partidos.png'))
    grafico_3_velocidad(speeds, os.path.join(output_dir, 'grafico_velocidad.png'))
    grafico_4_colisiones(collisions, os.path.join(output_dir, 'grafico_colisiones.png'))
    
    print(f"\n✅ 4 gráficos guardados en: {output_dir}\n")
    
    # Resumen
    print(f"{'='*50}")
    print(f"  Resumen")
    print(f"{'='*50}")
    if goals is not None and not goals.empty:
        print(f"   Goles totales: {len(goals)}")
        print(f"   Goles Red: {len(goals[goals['team_name'] == 'Red'])}")
        print(f"   Goles Blue: {len(goals[goals['team_name'] == 'Blue'])}")
    if speeds is not None and not speeds.empty:
        speeds['speed_px_per_sec'] = pd.to_numeric(speeds['speed_px_per_sec'], errors='coerce')
        print(f"   Velocidad máxima: {speeds['speed_px_per_sec'].max():.0f} px/s")
    print()


if __name__ == '__main__':
    main()
