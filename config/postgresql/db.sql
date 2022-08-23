CREATE SEQUENCE builtin_id_sequence MAXVALUE 999999;

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
	start_time timestamp,
	end_time timestamp,
	duration bigint,
	virtual bool,
	exam bool,
	deleted bool
);

CREATE TABLE kims_tasks (
	kim_id bigint,
	task_id bigint,
	pos integer,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (task_id) REFERENCES tasks(id),
	UNIQUE (kim_id, pos)
);

CREATE TABLE users (
	id bigint DEFAULT nextval('builtin_id_sequence') NOT NULL PRIMARY KEY,
	username text,
	display_name text,
	permissions integer,
	salt char(64),
	password char(64), -- = b16encode(sha3_256(password + b16decode(salt)))
	last_login_time timestamp,
	last_login_method text
);

CREATE TABLE sessions (
	id char(64) NOT NULL PRIMARY KEY,
	user_id bigint,
	login_time bigint, 

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
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

	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE
);

CREATE TABLE users_kims (
	user_id bigint,
	kim_id bigint,
	start_time timestamp,
	end_time timestamp,
	state int,

	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE
);

CREATE TABLE users_answers (
	kim_id bigint,
	pos integer,
	user_id bigint,
	answer bytea,
	submit_time bigint,

	FOREIGN KEY (kim_id, pos) REFERENCES kims_tasks(kim_id, pos) ON DELETE CASCADE,
	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE users_results (
	kim_id bigint,
	user_id bigint,
	points double precision[],
	total double precision,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE standings (
	kim_id bigint,
	group_id bigint,
	name text,
	fs_filename text,
	creation_time timestamp,

	FOREIGN KEY (kim_id) REFERENCES kims(id) ON DELETE CASCADE,
	FOREIGN KEY (group_id) REFERENCES groups(id) ON DELETE CASCADE
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

COPY task_types (obsolete, short_name, full_name, grading, scale_factor, deleted) FROM stdin;
f	1	1. Анализ информационных моделей	1	1	f
f	2	2. Таблицы истинности логических выражений	1	1	f
f	3	3. Поиск и сортировка в базах данных	1	1	f
f	4	4. Кодирование и декодирование данных. Условие Фано	1	1	f
f	5	5. Анализ алгоритмов для исполнителей	1	1	f
f	6	6. Анализ программ с циклами	1	1	f
f	7	7. Кодирование графической и звуковой информации	1	1	f
f	8	8. Комбинаторика	1	1	f
f	9	9. Обработка числовой информации в электронных таблицах	1	1	f
f	10	10. Поиск слова в текстовом документе	1	1	f
f	11	11. Вычисление количества информации	1	1	f
f	12	12. Алгоритмы для исполнителей с циклами и ветвлениями	1	1	f
f	13	13. Количество путей в ориентированном графе	1	1	f
f	14	14. Позиционные системы счисления	1	1	f
f	15	15. Истинность логического выражения	1	1	f
f	16	16. Вычисление значения рекурсивной функции	1	1	f
f	17	17. Обработка целочисленных данных. Проверка делимости	1	1	f
f	18	18. Динамическое программирование в электронных таблицах	1	1	f
f	19	19. Теория игр	1	1	f
f	20	20. Теория игр	1	1	f
f	21	21. Теория игр	1	1	f
f	22	22. Анализ программ с циклами и ветвлениями	1	1	f
f	23	23. Динамическое программирование (количество программ)	1	1	f
f	24	24. Обработка символьных строк	1	1	f
f	25	25. Обработка целочисленных данных. Поиск делителей	1	1	f
f	26	26. Обработка данных с помощью сортировки	3	1	f
f	27	27. Обработка потока данных	3	1	f
t	103	103. Поиск и сортировка в базах данных (Архив)	1	1	f
t	109	109. Обработка числовой информации в электронных таблицах (Архив)	1	1	f
t	117	117. Обработка целочисленных данных. Проверка делимости (Архив)	1	1	f
\.

INSERT INTO tasks (task_type, task, tag, answer_rows, answer_cols, answer, deleted) VALUES (
	29,
	'Имеется набор данных, состоящий из пар положительных целых чисел. Необходимо выбрать из каждой пары ровно одно число так, чтобы сумма всех выбранных чисел не делилась на 3 и при этом была максимально возможной. Гарантируется, что искомую сумму получить можно. Программа должна напечатать одно число – максимально возможную сумму, соответствующую условиям задачи.<br><br>Входные данные.<br>Даны два входных файла (файл A и файл B), каждый из которых содержит в первой строке количество пар <formula>n</formula> (<formula>1 \le n \le 100\,000</formula>). Каждая из следующих <formula>n</formula> строк содержит два натуральных числа, не превышающих <formula>10\,000</formula>.<br>Пример организации исходных данных во входном файле:<br>	Tab<br><pre>6<br>1 3<br>5 12<br>6 9<br>5 4<br>3 3<br>1 1<br></pre>Для указанных входных данных значением искомой суммы должно быть число 32. В ответе укажите два числа: сначала значение искомой суммы для файла А, затем для файла B.<br><br><b>Предупреждение: </b>для обработки файла B <b>не следует </b>использовать переборный алгоритм, вычисляющий сумму для всех возможных вариантов, поскольку написанная по такому алгоритму программа будет выполняться слишком долго.',
	'kompege.ru task:23',
	1,
	2,
	'127127\000399762080'::bytea,
	false
);

INSERT INTO kims (name, duration, virtual, exam, start_time, end_time) VALUES (
	'Тестовый вариант',
	235 * 60,
	false,
	false,
	'2022-08-22 14:00:00',
	'2022-08-22 18:55:00'
);

INSERT INTO kims_tasks VALUES (34, 33, 0);
