# Entrega 0.1 — aplicativo inicial

## Escopo entregue

Aplicativo nativo para macOS Apple Silicon, com interface organizada segundo os
grupos de trabalho familiares no Fusion, modelagem sólida baseada em Open CASCADE,
sketches básicos, histórico editável, arquivos nativos e exportação STEP/STL/DXF.

O objetivo operacional desta versão é criar e editar peças simples: placas,
suportes, caixas abertas por subtração, espaçadores, flanges e peças impressas.

## Verificação

- Testes geométricos de volumes de extrusão, revolução, furos e booleanas.
- Edição de sketch seguida de recálculo dos sólidos dependentes.
- Reversão automática de alterações inválidas e proteção de dependências.
- Desfazer/refazer e salvamento/reabertura do histórico.
- STEP exportado e reimportado, com comparação de volume e validade geométrica.
- STL exportado e reimportado com o leitor do núcleo geométrico.
- Sketches em XY, XZ e YZ, incluindo extrusão negativa.
- Exportação de círculos e arcos em DXF.
- Teste de interface com cliques reais de Qt: criar sketch, desenhar retângulo,
  concluir, extrudar, desfazer/refazer e reabrir projeto.
- Inspeção visual do modelo de exemplo e da visualização de sketch com cotas.
- DXF do exemplo aberto e auditado com ezdxf: quatro entidades, unidade mm,
  zero erros e zero correções necessárias.
- Pacote macOS aberto separadamente do build de desenvolvimento, com geração de
  STEP/STL/DXF e bibliotecas carregadas de dentro do pacote ou do sistema.

Os testes cobrem esses casos de referência, não todos os modelos de engenharia.

## Diferenças em relação ao escopo inicialmente discutido

O planejamento inicial era amplo demais para tratar todos os itens como uma
primeira entrega já concluída. Esta versão implementa o fluxo básico completo,
com estas pendências explícitas:

- Solver geral de restrições, graus de liberdade e cotas relacionais entre entidades.
- Trim/extend, offset, espelhamento, múltiplos perfis e ilhas no mesmo sketch.
- Anexar sketch a uma face e manter essa referência após mudanças topológicas.
- Prévia interativa dos comandos e manipuladores 3D de arraste.
- Seleção de faces/arestas; o filete atual é aplicado a todas as arestas do corpo.
- Chamfer, reorder/rollback da timeline, seleção múltipla e menu radial por gestos.
- Distância/ângulo entre entidades; Measure atual mostra caixa envolvente e volume.
- Temas completos, remapeamento de atalhos e múltiplos documentos simultâneos.
- Recálculo e importação em segundo plano para projetos grandes.
- Montagens, simulação, desenho técnico, CAM e Windows validado.

Os nomes e posições principais são familiares, mas não se afirma igualdade
integral de UI/UX com o Fusion. A estrutura já serve de base para aproximar os
fluxos restantes conforme os testes internos da empresa.

## Próximo incremento recomendado

Concluir o editor de sketch: solver de restrições, edição direta de cotas,
trim/extend, múltiplos contornos e seleção de face para iniciar um sketch.
