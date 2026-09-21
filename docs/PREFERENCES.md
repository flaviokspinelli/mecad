# Preferências persistentes

O aplicativo guarda quatro escolhas locais entre sessões:

- fundo claro/escuro, em View → Light canvas ou no menu de visualização;
- arestas visíveis, no menu de visualização;
- snap da grade, no menu de grade;
- encaixe inteligente, no menu de grade.

As escolhas são gravadas em `preferences.ini`, na pasta de configuração do
aplicativo fornecida pelo macOS/Qt. Não ficam no `.mcad`, não marcam o desenho
como alterado e não entram no histórico de desfazer. Testes usam arquivos isolados
em diretórios temporários, sem gravar nas preferências normais do usuário.

Ausência de uma chave ou valor booleano inválido usa o padrão: fundo escuro,
arestas, snap e encaixe inteligente ligados. Se não for possível gravar, a escolha
continua valendo na sessão e a barra de status informa a falha. Não há promessa de
sincronização ao vivo entre várias instâncias abertas.

Unidades, idioma, atalhos configuráveis e perfis de navegação ainda não fazem
parte dessas preferências persistentes. Portanto PROD-05 permanece parcial.
