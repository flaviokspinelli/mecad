# Backlog do MecaCAD

## Estado após a entrega 0.2

Revisão de interface e interação: tema escuro compacto, Browser sobre o canvas,
cubo clicável, histórico por ícones, plano por clique, extrusão e translação por
arraste com prévia cancelável. Veja [ENTREGA-0.2.md](docs/ENTREGA-0.2.md).
Próximas prioridades de mouse: arrastar vértices e cotas, anéis de rotação,
seleção individual de faces/arestas, furo por clique e menu radial.

A primeira implementação está em C++20, Qt Widgets e Open CASCADE, compilada e
testada no macOS Apple Silicon. O plano Windows inicial não havia sido confirmado;
a portabilidade e o teste Windows continuam pendentes. O nome MecaCAD é provisório.

Consulte [ENTREGA-0.1.md](docs/ENTREGA-0.1.md) para o que foi testado e as diferenças
entre o planejamento amplo e o aplicativo inicial. Itens parcialmente implementados
permanecem abertos abaixo. Prioridade seguinte: completar o editor de sketch e seus
fluxos de interação, mantendo a organização de menus familiar ao Fusion.

## Visão do produto

Criar um CAD paramétrico desktop para mecatrônica, econômico e extensível. A
interface deve preservar a memória muscular de quem usa Fusion 360, sem copiar
marca, ícones, recursos gráficos ou código proprietário.

## Princípios

- Entregar incrementos pequenos e utilizáveis.
- Validar a experiência antes de aprofundar o motor geométrico.
- Usar componentes abertos e consolidados quando possível.
- Manter arquivos do usuário portáveis e documentados.
- Separar interface, modelo paramétrico e núcleo geométrico.
- Tratar simulações iniciais como exploratórias até que sejam validadas.

## Prioridades

- **P0:** necessário para o primeiro produto utilizável.
- **P1:** próximo incremento de alto valor.
- **P2:** evolução posterior.
- **P3:** ideia para avaliação futura.

## Marco 0 — Fundação do projeto

- [ ] **P0** Confirmar nome definitivo e identidade visual.
- [ ] **P0** Confirmar Windows como primeira plataforma.
- [ ] **P0** Definir licenças das dependências e do código interno.
- [x] **P0** Configurar CMake, C++ 20, Qt 6 e testes automatizados.
- [x] **P0** Definir arquitetura modular e convenções do código.
- [ ] **P0** Criar integração contínua para compilação e testes.
- [ ] **P1** Preparar empacotamento e instalador de desenvolvimento.

### Critério de aceite

O projeto compila em uma máquina limpa, abre uma janela vazia e executa os
testes básicos automaticamente.

## Marco 1 — Protótipo da experiência

- [x] **P0** Criar janela principal e viewport 3D.
- [x] **P0** Criar barra superior contextual.
- [ ] **P0** Criar navegador de documentos e componentes à esquerda.
- [x] **P0** Criar painel de propriedades à direita.
- [x] **P0** Criar histórico paramétrico na parte inferior.
- [x] **P0** Implementar órbita, pan e zoom.
- [x] **P0** Implementar seleção e realce visual (corpos e contornos de sketches).
- [x] **P0** Criar cubo de orientação e vistas padrão.
- [ ] **P1** Implementar temas escuro e claro.
- [ ] **P1** Criar atalhos configuráveis.
- [x] **P1** Criar menus contextuais próximos ao cursor (menu radial pendente).
- [ ] **P1** Garantir suporte a telas de alta resolução.

### Critério de aceite

O usuário consegue navegar no ambiente 3D, selecionar objetos demonstrativos e
reconhecer imediatamente a organização principal do aplicativo.

## Marco 2 — Modelagem sólida básica

- [x] **P0** Integrar o Open CASCADE.
- [x] **P0** Criar caixa, cilindro e esfera por parâmetros.
- [x] **P0** Editar dimensões e posição pelo painel de propriedades.
- [x] **P0** Mover, rotacionar, copiar e excluir objetos.
- [x] **P0** Implementar desfazer e refazer.
- [x] **P0** Implementar união, subtração e interseção.
- [x] **P0** Salvar e abrir o formato nativo do projeto.
- [x] **P0** Exportar STL.
- [x] **P0** Exportar e importar STEP.
- [ ] **P1** Medir distância, ângulo, raio e volume.
- [x] **P1** Detectar falhas geométricas e apresentar mensagens úteis.

### Critério de aceite

O usuário cria uma peça usando primitivas e operações booleanas, altera seus
parâmetros, salva o projeto e exporta um STL e um STEP válidos.

## Marco 3 — Sketch paramétrico

- [ ] **P0** Criar sketch em planos globais ou faces planas.
- [x] **P0** Desenhar linha, retângulo, círculo e arco.
- [ ] **P0** Aplicar cotas lineares, angulares, radiais e de diâmetro.
- [ ] **P0** Aplicar restrições coincidente, horizontal e vertical.
- [ ] **P0** Aplicar restrições paralela, perpendicular e tangente.
- [ ] **P0** Exibir graus de liberdade restantes.
- [x] **P0** Finalizar e reabrir um sketch.
- [x] **P0** Extrudar perfil fechado com adição ou remoção.
- [x] **P1** Implementar revolução.
- [x] **P1** Implementar ferramenta de furo.
- [ ] **P1** Implementar filete e chanfro.
- [x] **P1** Recalcular o histórico após alterar uma cota.
- [ ] **P2** Implementar padrões lineares e circulares.
- [ ] **P2** Implementar loft e sweep.

### Critério de aceite

O usuário desenha um perfil cotado, extruda uma peça, cria um furo e modifica a
geometria retornando às cotas ou às operações do histórico.

## Marco 4 — Fluxo para mecatrônica

- [ ] **P1** Criar documentos de montagem.
- [ ] **P1** Inserir componentes e submontagens.
- [ ] **P1** Criar juntas fixa, rotativa e linear.
- [ ] **P1** Definir limites de movimento.
- [ ] **P1** Detectar interferências e colisões.
- [ ] **P1** Gerar lista de materiais.
- [ ] **P1** Criar parâmetros globais e fórmulas.
- [ ] **P2** Criar biblioteca de motores, rolamentos e parafusos.
- [ ] **P2** Criar biblioteca de sensores, conectores e perfis.
- [ ] **P2** Associar pontos elétricos e mecânicos aos componentes.
- [ ] **P2** Importar bibliotecas personalizadas da empresa.

### Critério de aceite

O usuário monta um mecanismo simples, limita seu movimento, verifica colisões e
gera uma lista de materiais.

## Marco 5 — Desenho técnico e fabricação

- [ ] **P1** Criar folha de desenho e carimbo configurável.
- [ ] **P1** Gerar vistas ortográficas e isométricas.
- [ ] **P1** Adicionar cotas e anotações.
- [ ] **P1** Exportar desenho em PDF.
- [ ] **P1** Exportar sketch e desenho em DXF.
- [ ] **P2** Adicionar ferramentas básicas para chapas.
- [ ] **P2** Preparar modelos para impressão 3D.
- [ ] **P3** Criar ambiente CAM e simulação de percurso CNC.

### Critério de aceite

O usuário gera um desenho cotado e exporta arquivos PDF, DXF e STL adequados ao
processo de fabricação selecionado.

## Marco 6 — Simulação

- [ ] **P2** Definir materiais e propriedades mecânicas.
- [ ] **P2** Configurar apoios e forças.
- [ ] **P2** Gerar e inspecionar malha.
- [ ] **P2** Integrar um solucionador estrutural aberto.
- [ ] **P2** Visualizar deslocamento, tensão e fator de segurança.
- [ ] **P2** Emitir alertas sobre hipóteses e limitações do estudo.
- [ ] **P3** Adicionar análise térmica.
- [ ] **P3** Adicionar análise de movimento.
- [ ] **P3** Avaliar análise de fluidos.

### Critério de aceite

O usuário executa uma análise estrutural estática demonstrativa, entende as
condições aplicadas e visualiza os resultados com unidades e legenda claras.

## Requisitos transversais

- [x] Manter unidade mm e geometria em precisão dupla; visualização usa malha.
- [ ] Evitar perda de dados em falhas e oferecer recuperação automática.
- [ ] Preservar compatibilidade de arquivos entre versões.
- [ ] Manter operações demoradas fora da interface principal.
- [ ] Registrar erros técnicos sem expor complexidade desnecessária ao usuário.
- [x] Criar testes para arquivos, geometria, histórico e exportadores.
- [x] Documentar atalhos, formatos e fluxos principais.
- [ ] Verificar licenças antes de incorporar qualquer dependência.

## Primeira sequência de execução

1. Confirmar nome, plataforma inicial e escopo do primeiro protótipo.
2. Montar a fundação C++/Qt/CMake.
3. Criar a interface navegável com objetos demonstrativos.
4. Validar visual e interação com usuários internos.
5. Integrar o núcleo geométrico e entregar as primeiras primitivas.
6. Implementar persistência, STL e STEP.
7. Iniciar o editor de sketch paramétrico.

## Fora do primeiro lançamento

- Colaboração em nuvem em tempo real.
- Aplicativos móveis completos.
- Simulação CFD profissional.
- CAM multieixos avançado.
- Compatibilidade direta com arquivos nativos proprietários do Fusion 360.
- Certificação de resultados para engenharia regulamentada.
