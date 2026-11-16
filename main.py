
import serial
import time
from datetime import datetime
from pathlib import Path

# Configurações da comunicação
PORTA = "COM3"
BAUDRATE = 115200
TOTAL_AMOSTRAS = 1500
TIMEOUT_ENSAIO = 90

# Alturas do CENTRO da esfera em centímetros
ALTURAS_CM = [
    0.50, 0.75, 1.00, 1.25,
    1.50, 1.75, 2.00, 2.25,
    2.50, 2.75, 3.00, 3.25, 3.50
]

# Arquivo de saída
ARQUIVO = Path(__file__).resolve().with_name("dados.txt")
CABECALHO = "data_hora,altura_cm,tipo,tempo_ms,pwm,amostra,sensor1,sensor2,media"


def verificarArquivo():
    if ARQUIVO.exists() and ARQUIVO.stat().st_size > 0:
        with ARQUIVO.open("r", encoding="utf-8") as arquivo:
            primeiraLinha = arquivo.readline().strip()

        if primeiraLinha != CABECALHO:
            raise RuntimeError(
                "O arquivo dados.txt já existe em outro formato. "
                "Renomeie ou remova o arquivo antigo."
            )


def coletarAmostras(esp):
    amostras = []
    iniciado = False

    esp.reset_input_buffer()
    esp.write(b"1\n")
    esp.flush()

    print("\n>>> COMANDO DE INÍCIO ENVIADO")

    limite = time.monotonic() + TIMEOUT_ENSAIO

    while time.monotonic() < limite:
        linha = esp.readline().decode("utf-8", errors="replace").strip()

        if not linha:
            continue

        if linha == "ENSAIO_INICIADO":
            iniciado = True
            print(">>> AQUISIÇÃO INICIADA")

        elif linha.startswith("DATA,") and iniciado:
            campos = linha.split(",")

            if len(campos) != 7:
                raise RuntimeError(f"Amostra inválida: {linha}")

            try:
                tempo = int(campos[1])
                pwm = int(campos[2])
                indice = int(campos[3])
                sensor1 = int(campos[4])
                sensor2 = int(campos[5])
                media = float(campos[6])

            except ValueError:
                raise RuntimeError(f"Dados inválidos: {linha}")

            if pwm != 0:
                raise RuntimeError("PWM diferente de zero!")

            if indice != len(amostras) + 1:
                raise RuntimeError(
                    f"Sequência incorreta! Esperado: {len(amostras) + 1}, "
                    f"Recebido: {indice}"
                )

            if len(amostras) >= TOTAL_AMOSTRAS:
                raise RuntimeError("Quantidade de amostras excedida!")

            dataHora = datetime.now().isoformat(
                sep=" ", timespec="milliseconds"
            )

            amostras.append((dataHora, linha))

            if len(amostras) % 250 == 0:
                print(
                    f">>> AMOSTRAS COLETADAS: "
                    f"{len(amostras)}/{TOTAL_AMOSTRAS}"
                )

        elif linha == "ENSAIO_FINALIZADO" and iniciado:
            if len(amostras) != TOTAL_AMOSTRAS:
                raise RuntimeError(
                    f"Ensaio incompleto: "
                    f"{len(amostras)}/{TOTAL_AMOSTRAS}"
                )

            print(">>> AQUISIÇÃO FINALIZADA!")
            return amostras

    raise RuntimeError("Tempo limite de aquisição excedido!")


def salvarAmostras(altura, amostras):
    novoArquivo = not ARQUIVO.exists() or ARQUIVO.stat().st_size == 0

    with ARQUIVO.open("a", encoding="utf-8", newline="") as arquivo:
        if novoArquivo:
            arquivo.write(CABECALHO + "\n")

        for dataHora, linha in amostras:
            arquivo.write(f"{dataHora},{altura:.2f},{linha}\n")

    print(f">>> {len(amostras)} AMOSTRAS GRAVADAS!")
    print(f">>> ALTURA DO CENTRO: {altura:.2f} cm")
    print(f">>> ARQUIVO: {ARQUIVO.name}")


def main():
    verificarArquivo()

    with serial.Serial(PORTA, BAUDRATE, timeout=1) as esp:
        time.sleep(2)
        esp.reset_input_buffer()

        print("\n====================================")
        print("  CALIBRAÇÃO DOS SENSORES HALL")
        print("====================================")
        print("PWM: 0")
        print(f"AMOSTRAS POR ALTURA: {TOTAL_AMOSTRAS}")
        print(f"TOTAL DE ALTURAS: {len(ALTURAS_CM)}")
        print(f"TOTAL PREVISTO: {len(ALTURAS_CM) * TOTAL_AMOSTRAS}")
        print("REFERÊNCIA: CENTRO DA ESFERA")
        print("====================================")
        print("1 = Iniciar aquisição")
        print("sair = Encerrar")
        print("====================================")

        for numero, altura in enumerate(ALTURAS_CM, start=1):
            while True:
                print("\n------------------------------------")
                print(f"ETAPA {numero}/{len(ALTURAS_CM)}")
                print(f"POSICIONE A ESFERA EM {altura:.2f} cm")
                print("------------------------------------")

                comando = input("Digite 1 para iniciar: ").strip().lower()

                if comando == "sair":
                    print("\n>>> PROGRAMA ENCERRADO!")
                    return

                if comando != "1":
                    print(">>> Comando inválido!")
                    continue

                try:
                    amostras = coletarAmostras(esp)
                    salvarAmostras(altura, amostras)

                    print(">>> ETAPA CONCLUÍDA COM SUCESSO!")
                    break

                except RuntimeError as erro:
                    print(f"\n>>> ERRO: {erro}")
                    print(">>> Dados desta etapa não foram gravados.")
                    print(">>> Aguarde o ESP32 finalizar antes de repetir.")

                    time.sleep(2)
                    esp.reset_input_buffer()

        print("\n====================================")
        print("     CALIBRAÇÃO FINALIZADA!")
        print("====================================")
        print(f"ALTURAS TESTADAS: {len(ALTURAS_CM)}")
        print(f"TOTAL DE AMOSTRAS: {len(ALTURAS_CM) * TOTAL_AMOSTRAS}")
        print(f"ARQUIVO: {ARQUIVO}")
        print("====================================")


if __name__ == "__main__":
    try:
        main()

    except KeyboardInterrupt:
        print("\n>>> Programa interrompido pelo usuário.")

    except serial.SerialException as erro:
        print(f"\n>>> Erro na comunicação Serial: {erro}")

    except RuntimeError as erro:
        print(f"\n>>> Erro: {erro}")
