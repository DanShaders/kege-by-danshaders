CREATE SEQUENCE builtin_id_sequence MAXVALUE 499999;

CREATE TABLE api_id_sequence (
	value bigint
);
INSERT INTO api_id_sequence VALUES (1000000);

CREATE TABLE task_types (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	obsolete bool,
	short_name integer,
	full_name text,
	grading integer,
	scale_factor double precision CHECK (scale_factor > 0),
	deleted bool
);

CREATE TABLE tasks (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	task_type bigint,
	parent bigint,
	task text,
	tag text,
	answer_rows integer CHECK (answer_rows > 0 AND answer_rows <= 10),
	answer_cols integer CHECK (answer_cols > 0 AND answer_cols <= 10),
	answer bytea,
	deleted bool,

	FOREIGN KEY (task_type) REFERENCES task_types(id),
	FOREIGN KEY (parent) REFERENCES tasks(id)
);

CREATE TABLE task_attachments (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	task_id bigint,
	filename text,
	hash text,
	mime_type text,
	shown_to_user bool,
	deleted bool,

	FOREIGN KEY (task_id) REFERENCES tasks(id) ON DELETE CASCADE
);

CREATE TABLE kims (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	name text,
	token_version bigint,
	deleted bool
);

CREATE TABLE kims_tasks (
	kim_id bigint,
	task_id bigint,
	pos integer,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (task_id) REFERENCES tasks(id),
	UNIQUE (kim_id, pos) DEFERRABLE INITIALLY DEFERRED,
	UNIQUE (kim_id, task_id) -- do not lift unless DiffableSet stops depend on this
);

CREATE TABLE users (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	username text,
	display_name text,
	permissions integer,
	salt char(64),
	password char(64) -- = b16encode(sha3_256(password + b16decode(salt)))
);

CREATE TABLE groups (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	display_name text
);

CREATE TABLE users_groups (
	user_id bigint,
	group_id bigint,

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
);

CREATE TABLE groups_kims (
	group_id bigint,
	kim_id bigint,
	start_time timestamp,
	end_time timestamp,
	duration bigint,
	virtual bool,
	exam bool,

	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	UNIQUE (group_id, kim_id)
);

CREATE TABLE users_kims (
	user_id bigint,
	kim_id bigint,
	start_time timestamp,
	end_time timestamp,

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	UNIQUE (user_id, kim_id)
);

CREATE TABLE users_answers (
	kim_id bigint,
	task_id bigint,
	user_id bigint,
	answer bytea,
	score double precision,
	submit_time bigint,

	FOREIGN KEY (kim_id, task_id) REFERENCES kims_tasks(kim_id, task_id) ON DELETE CASCADE,
	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	UNIQUE (user_id, answer_time) -- Rejudging depends on this
);

CREATE TABLE jobs (
	id bigint,
	type integer,
	status bytea,
	data bytea
);

-- admin:password
INSERT INTO users (username, display_name, permissions, salt, password) VALUES (
	'admin',
	'Administrator',
	2147483647,
	'8a339ca48b640a58dfac3c8f40963e7ed99ad0f525146f8c1b6a14fe6b07adf7',
	'4db6fac3b8811822e57a926e9ff52b5e1aea0bd52e58125e0d5fccc45d0c0057'
);

--user:password
INSERT INTO users (username, display_name, permissions, salt, password) VALUES (
	'user',
	'User',
	0,
	'a93c87f2f88b9aca33ba64fdb35bde919c0aadfd690507d9b2da38cb1a9ba4ff',
	'e1bdae5584062bf6a4472260180be513e1db4cfe334fec1c67cb5e5eb26452fa'
);

COPY task_types (id, obsolete, short_name, full_name, grading, scale_factor, deleted) FROM stdin;
500000	f	1	1. Анализ информационных моделей	1	1	f
500001	f	2	2. Таблицы истинности логических выражений	1	1	f
500002	f	3	3. Поиск и сортировка в базах данных	1	1	f
500003	f	4	4. Кодирование и декодирование данных. Условие Фано	1	1	f
500004	f	5	5. Анализ алгоритмов для исполнителей	1	1	f
500005	f	6	6. Циклические алгоритмы для Исполнителя	1	1	f
500006	f	7	7. Кодирование графической и звуковой информации	1	1	f
500007	f	8	8. Комбинаторика	1	1	f
500008	f	9	9. Обработка числовой информации в электронных таблицах	1	1	f
500009	f	10	10. Поиск слова в текстовом документе	1	1	f
500010	f	11	11. Вычисление количества информации	1	1	f
500011	f	12	12. Алгоритмы для исполнителей с циклами и ветвлениями	1	1	f
500012	f	13	13. Количество путей в ориентированном графе	1	1	f
500013	f	14	14. Позиционные системы счисления	1	1	f
500014	f	15	15. Истинность логического выражения	1	1	f
500015	f	16	16. Вычисление значения рекурсивной функции	1	1	f
500016	f	17	17. Обработка целочисленных данных. Проверка делимости	1	1	f
500017	f	18	18. Динамическое программирование в электронных таблицах	1	1	f
500018	f	19	19. Теория игр	1	1	f
500019	f	20	20. Теория игр	1	1	f
500020	f	21	21. Теория игр	1	1	f
500021	f	22	22. Многопоточные вычисления	1	1	f
500022	f	23	23. Динамическое программирование (количество программ)	1	1	f
500023	f	24	24. Обработка символьных строк	1	1	f
500024	f	25	25. Обработка целочисленных данных. Поиск делителей	1	1	f
500025	f	26	26. Обработка данных с помощью сортировки	3	2	f
500026	f	27	27. Обработка потока данных	3	2	f
500027	t	103	103. Поиск и сортировка в базах данных	1	1	f
500028	t	106	106. Анализ программ с циклами	1	1	f
500029	t	109	109. Обработка числовой информации в электронных таблицах	1	1	f
500030	t	117	117. Обработка целочисленных данных. Проверка делимости	1	1	f
500031	t	122	122. Анализ программ с циклами и ветвлениями	1	1	f
\.

INSERT INTO tasks (task_type, task, tag, answer_rows, answer_cols, answer, deleted) VALUES (
	500026,
	'Имеется набор данных, состоящий из пар положительных целых чисел. Необходимо выбрать из каждой пары ровно одно число так, чтобы сумма всех выбранных чисел не делилась на 3 и при этом была максимально возможной. Гарантируется, что искомую сумму получить можно. Программа должна напечатать одно число – максимально возможную сумму, соответствующую условиям задачи.<br><br>Входные данные.<br>Даны два входных файла (файл A и файл B), каждый из которых содержит в первой строке количество пар <formula>n</formula> (<formula>1 \le n \le 100\,000</formula>). Каждая из следующих <formula>n</formula> строк содержит два натуральных числа, не превышающих <formula>10\,000</formula>.<br>Пример организации исходных данных во входном файле:<br>	Tab<br><pre>6<br>1 3<br>5 12<br>6 9<br>5 4<br>3 3<br>1 1<br></pre>Для указанных входных данных значением искомой суммы должно быть число 32. В ответе укажите два числа: сначала значение искомой суммы для файла А, затем для файла B.<br><br><b>Предупреждение: </b>для обработки файла B <b>не следует </b>использовать переборный алгоритм, вычисляющий сумму для всех возможных вариантов, поскольку написанная по такому алгоритму программа будет выполняться слишком долго.',
	'kompege.ru task:23',
	1,
	2,
	'127127\000399762080'::bytea,
	false
);

INSERT INTO kims (name) VALUES ('Тестовый вариант');
INSERT INTO kims_tasks VALUES (4, 3, 0);
INSERT INTO kims_tasks VALUES (4, NULL, 1);

INSERT INTO groups (display_name) VALUES ('Тестовая группа');
INSERT INTO users_groups VALUES (1, 5);
INSERT INTO users_groups VALUES (2, 5);

INSERT INTO groups_kims VALUES (
	5,
	4,
	'2022-12-31 00:00:00',
	'2023-01-31 00:00:00',
	31::bigint * 86400 * 1000,
	false,
	false
);
