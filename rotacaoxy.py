import csv
import statistics
import time
from datetime import datetime
from pathlib import Path

import serial

PORTA = "COM3"
BAUD = 115200
AMOSTRAS = 300
ALTURA_REAL_CM = 2.00
PRONTO = "PRONTO,XY_Z,300,PWM_0"

# XY e o plano horizontal; rotacao apenas em torno do eixo Z (base -> eletroima).
ETAPAS = [
    ("XY_0_REF", "Alinhe a marca da esfera com a referencia inicial (0 graus)."),
    ("XY_90", "Gire em torno de Z cerca de 90 graus e pare."),
    ("XY_180", "Gire ate 180 graus em relacao a referencia e pare."),
    ("XY_270", "Gire ate 270 graus em relacao a referencia e pare."),
    ("XY_0_RETORNO", "Retorne a marca exatamente para 0 graus e pare."),
]

SAIDA = Path(__file__).resolve().parent / f"ensaio_xy_{datetime.now():%Y%m%d_%H%M%S}.txt"


def ler_linha(esp, limite=5):
    fim = time.monotonic() + limite
    while time.monotonic() < fim:
        linha = esp.readline().decode("utf-8", errors="replace").strip()
        if linha:
            return linha
    raise TimeoutError("ESP32 nao enviou uma linha dentro do tempo limite.")


def conectar(esp):
    print("Sincronizando com o ESP32...")
    time.sleep(2.0)  # A abertura da porta pode reiniciar o ESP32.
    esp.reset_input_buffer()
    prazo = time.monotonic() + 15
    proximo_ping = 0
    ultimas = []
    while time.monotonic() < prazo:
        if time.monotonic() >= proximo_ping:
            esp.write(b"H\n")
            esp.flush()
            proximo_ping = time.monotonic() + 0.6
        linha = esp.readline().decode("utf-8", errors="replace").strip()
        if linha:
            if linha == PRONTO:
                print("Conectado: " + linha)
                return
            ultimas.append(linha)
            ultimas = ultimas[-5:]
    diagnostico = " | ".join(ultimas) if ultimas else "nenhuma resposta"
    raise RuntimeError(
        "ESP32 nao confirmou o firmware XY. Confirme o upload do ensaio_xy_esp32.cpp "
        f"e a porta {PORTA}. Respostas recebidas: {diagnostico}"
    )


def iniciar_captura(esp):
    esp.reset_input_buffer()
    esp.write(b"1\n")
    esp.flush()
    prazo = time.monotonic() + 8
    while time.monotonic() < prazo:
        linha = ler_linha(esp, 2)
        if linha == f"INICIO,{AMOSTRAS}":
            return
        if linha.startswith("DADO,"):
            raise RuntimeError("Dados recebidos antes do INICIO; coleta dessincronizada.")
    raise TimeoutError("Nao chegou INICIO da coleta. Verifique o firmware do ESP32.")


def main():
    print(f"Ensaio XY (rotacao sobre Z) | PWM 0 | {AMOSTRAS} amostras/posicao")
    print(f"Altura FISICA fixa: {ALTURA_REAL_CM:.2f} cm. Nao gire DURANTE as capturas.")
    print(f"Arquivo TXT: {SAIDA}")

    with serial.Serial(PORTA, BAUD, timeout=0.25, write_timeout=2) as esp:
        conectar(esp)

        with SAIDA.open("w", encoding="utf-8", newline="") as arquivo:
            arquivo.write(f"# ENSAIO ROTACAO XY / EIXO Z | {datetime.now():%Y-%m-%d %H:%M:%S}\n")
            arquivo.write(f"# altura_real_cm={ALTURA_REAL_CM:.2f};pwm=0;amostras_por_posicao={AMOSTRAS}\n")
            arquivo.write("# Angulos sao posicoes manuais aproximadas, nao medidas por encoder.\n")
            arquivo.write("# O eixo Z vai da base ao eletroima. A esfera nao gira durante a captura.\n")
            gravador = csv.writer(arquivo, delimiter=";")
            gravador.writerow(["etapa", "amostra", "tempo_esp_ms", "s1_adc", "s2_adc", "media_adc", "diferenca_s1_s2"])
            arquivo.flush()
            resumos = []

            for n, (nome, instrucao) in enumerate(ETAPAS, 1):
                print(f"\n[{n}/{len(ETAPAS)}] {nome}: {instrucao}")
                input("Mantenha o centro na mesma altura e pressione ENTER para capturar...")
                iniciar_captura(esp)

                linhas = []
                for i in range(1, AMOSTRAS + 1):
                    mensagem = ler_linha(esp, 5)
                    dados = mensagem.split(",")
                    if len(dados) != 7 or dados[0] != "DADO" or int(dados[1]) != i:
                        raise RuntimeError(f"Amostra inesperada no passo {nome}: {mensagem!r}")

                    _, numero, instante, s1, s2, media, delta = dados
                    linha = (nome, int(numero), int(instante), int(s1), int(s2), float(media), int(delta))
                    gravador.writerow(linha)
                    linhas.append(linha)
                    if i % 50 == 0:
                        arquivo.flush()

                fim = ler_linha(esp, 5)
                if fim != "FIM":
                    raise RuntimeError(f"Fim inesperado em {nome}: {fim!r}")
                arquivo.flush()

                def estatistica(coluna):
                    valores = [linha[coluna] for linha in linhas]
                    return statistics.mean(valores), statistics.stdev(valores)

                s1_med, s1_dp = estatistica(3)
                s2_med, s2_dp = estatistica(4)
                media_med, media_dp = estatistica(5)
                delta_med, _ = estatistica(6)
                sat_s1 = sum(linha[3] == 4095 for linha in linhas)
                sat_s2 = sum(linha[4] == 4095 for linha in linhas)
                resumos.append((nome, s1_med, s1_dp, s2_med, s2_dp, media_med, media_dp, delta_med, sat_s1, sat_s2))
                print(f"Salvas {AMOSTRAS} amostras | S1={s1_med:.1f} | S2={s2_med:.1f} "
                      f"| Media={media_med:.1f} | DP={media_dp:.1f} | S1-S2={delta_med:+.1f}")

            arquivo.write("\n# RESUMO POR POSICAO\n")
            gravador.writerow(["etapa", "s1_media", "s1_dp", "s2_media", "s2_dp", "media_adc", "dp_media_adc", "media_s1_menos_s2", "s1_saturadas", "s2_saturadas"])
            for r in resumos:
                gravador.writerow([r[0], *(f"{x:.2f}" if isinstance(x, float) else x for x in r[1:])])

    print(f"\nENSAIO CONCLUIDO. Arquivo salvo em:\n{SAIDA}")


if __name__ == "__main__":
    try:
        main()
    except (KeyboardInterrupt, serial.SerialException, TimeoutError, RuntimeError, OSError, ValueError) as exc:
        print(f"\nEnsaio interrompido: {exc}")
        print(f"Confira o arquivo parcial, se existir: {SAIDA}")
