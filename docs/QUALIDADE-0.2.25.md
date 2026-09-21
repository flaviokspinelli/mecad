# Consolidação 0.2.25

## O que foi alterado

- Sketch criado sobre uma face guarda corpo de suporte, índice da face e contagem
  de faces. Ao reconstruir o histórico, seu plano é recalculado. Isso permite
  alterar a altura de uma caixa e manter um corte desenhado sobre ela.
- Fillet aceita arestas selecionadas com Shift no mesmo corpo. Selecionar o corpo
  inteiro mantém o modo de todas as arestas. A prévia não altera o documento;
  raios inválidos bloqueiam a confirmação; Cancel descarta a operação.
- Measure calcula área de face, comprimento de aresta e distância mínima entre
  dois elementos CAD. A medição não estima o comprimento por segmentos de tela.
- Orientações antes escondidas agora aparecem na barra de estado.
- Exclusão recusada por dependências deixou de inserir campos nulos no documento.

## Validação

Em 21/09/2026, compilação Release concluída e `ctest --test-dir build
--output-on-failure --timeout 90` aprovado: suítes core e ui, zero falhas.
As duas execuções completas finais passaram (33,48 s e 43,24 s).

Testes nativos Qt, com eventos de mouse, ações e painéis do aplicativo, além de
testes geométricos Open CASCADE. A suíte verifica:

- Desenhar, extrudar por arraste contínuo, reduzir a prévia, confirmar e cancelar.
- Selecionar a face, desenhar um retângulo, finalizar, selecionar o perfil sobre
  o sólido, cortar, desfazer/refazer, salvar/reabrir e exportar STEP/STL.
- Alterar dimensões do suporte e reconstruir sketch e corte associados.
- Selecionar aresta, visualizar filete, recusar raio inválido, confirmar, desfazer
  e refazer. Cancelar um filete não muda o documento.
- Medir uma face de 900 mm² e a distância de 10 mm entre vértices com Shift.
- Salvar/reabrir filete e comparar o volume após exportar/importar STEP.
- Regressões de seleção por área, Shift, vértices, dimensões, snapping, navegação,
  movimento, rotação, importação STL e rollback.

Os testes de fluxo que enviam eventos diretamente aos widgets não exigem mais
que a janela de teste seja a janela ativa do macOS. O teste de foco do editor de
cotas continua verificando o foco explicitamente. Capturas ficam em `build/`.

## Limitações importantes

Esta versão não equivale ao Fusion e não conclui a maturidade do produto.

- Ainda não há solucionador geral de restrições geométricas, montagens/juntas,
  simulação, loft/sweep ou desenhos técnicos completos.
- Referências de face e aresta usam índices topológicos, não identificação
  persistente robusta. Mudanças na quantidade de faces do suporte são recusadas;
  reorganizações que mantenham a contagem exigem inspeção. Não é garantida a
  preservação de referência para alterações topológicas arbitrárias.
- Sketches antigos sem suporte continuam com plano fixo. O vínculo acompanha a
  etapa de suporte escolhida, não uma movimentação posterior de outra etapa.
- STL continua sendo malha; não suporta sketch em face ou filete CAD.
- Filete não tem seleção dinâmica de novas arestas dentro do painel: cancele,
  altere a seleção e abra novamente. O raio usa campo numérico com prévia.
- Operações geométricas ainda são síncronas; modelos complexos podem bloquear a
  interface. Os testes não certificam desempenho ou segurança para fabricação.

## Próximos critérios de maturidade

1. Restrições e cotas gerais do sketch, com diagnóstico de sobre/sub-restrição.
2. Referências geométricas persistentes e reparo de referências rompidas.
3. Perfis compostos e seleção de regiões de sketch.
4. Execução cancelável de operações geométricas demoradas.
5. Validação continuada com conjuntos reais de peças mecatrônicas.
