# MecaCAD 0.1

CAD desktop para peças mecatrônicas simples. Implementação independente em C++20,
Qt Widgets 6 e Open CASCADE 7.9. A versão inicial executa modelagem real e foi
desenvolvida e testada em macOS 14, Apple Silicon.

![Interface](docs/preview.png)

## Abrir

Abra `dist/MecaCAD.app`. O pacote local reúne o executável e suas bibliotecas.
O código-fonte não depende da pasta `dist`, que é gerada e ignorada pelo Git.

Para começar: **File → Open example — mounting bracket**. Também existem os
arquivos `examples/Mounting-bracket.mcad`, `.step`, `.stl` e `Base-profile.dxf`.

Leia o [guia de uso](docs/GUIA.md) e o [estado da entrega](docs/ENTREGA-0.1.md).

## Implementado

- Barra de comandos organizada em Create, Modify, Assemble, Construct, Inspect,
  Insert e Select, com troca contextual para Sketch.
- Browser à esquerda, edição à direita e histórico embaixo.
- Visualização 3D com OpenGL, seleção de corpos e contornos de sketches, pan,
  órbita, zoom, vistas ortográficas, ajuste de enquadramento e trackpad.
- Sketch em XY, XZ ou YZ, com offset numérico; retângulo, círculo, polilinha e arco.
- Dimensões editáveis de retângulo/círculo e coordenadas editáveis de polilinhas.
- Extrusão e revolução de perfil fechado, novo corpo, união e corte.
- Caixa, cilindro, esfera, furos, booleanas, mover, copiar, rotacionar e filete
  aplicado a todas as arestas.
- Histórico com recálculo, desfazer/refazer, salvar/reabrir e recuperação periódica.
- Importação/exportação STEP, exportação STL e DXF dos sketches.
- Medidas da caixa envolvente e volume do sólido.

Os itens de menu identificados como **planned** estão desabilitados. A estrutura
familiar não significa equivalência total de interface ou funcionalidades ao Fusion.

## Limites atuais

A versão 0.1 tem um perfil por sketch. Retângulos e círculos têm parâmetros
dimensionais; ainda não há um solucionador geral de restrições, trim/extend,
sketch ligado a face, seleção individual de arestas para filete, montagens,
simulação, desenhos técnicos, CAM ou importação de arquivos nativos do Fusion.
O histórico pode ser editado, mas não reordenado ou percorrido com rollback.
STEP preserva geometria, não o histórico paramétrico do aplicativo de origem.
O recálculo é síncrono e a versão é indicada para modelos pequenos.

## Compilar no macOS

Dependências usadas: Qt 6.11.2 e Open CASCADE 7.9.3. O renderizador requer OpenGL 3.2.

```sh
brew install cmake qtbase opencascade
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qtbase;/opt/homebrew/opt/opencascade"
cmake --build build -j 6
ctest --test-dir build --output-on-failure
open build/MecaCAD.app
```

Para gerar o pacote com bibliotecas e ZIP: `sh scripts/package-macos.sh`.

O teste de interface requer uma sessão gráfica. A versão Windows ainda não foi
compilada nem validada; o CMake usa dependências Qt/Open CASCADE disponíveis em
outras plataformas, mas isso não substitui o teste nessas plataformas.

## Arquitetura

- `mecacore`: documento, operações paramétricas, núcleo geométrico e exportadores.
- `mecaui`: interface Qt Widgets e viewport OpenGL.
- `MecaCAD`: aplicativo desktop.
- `core_tests`: testes de geometria, arquivos e recálculo.
- `ui_tests`: fluxo de sketch → sólido por cliques na interface.
- `.mcad`: JSON versionado em milímetros; geometria importada é incorporada ao arquivo.

O histórico usa identificadores estáveis entre operações. Cada edição é validada
antes de entrar no histórico de desfazer. O salvamento nativo usa escrita atômica.
Qt Widgets foi adotado nesta etapa no lugar de QML para integrar diretamente os
controles desktop e o viewport, preservando a separação entre interface e modelo.

[Backlog](BACKLOG.md) · [Dependências](docs/DEPENDENCIAS.md)
