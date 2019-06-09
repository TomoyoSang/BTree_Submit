#include "utility.hpp"
#include <functional>
#include <cstddef>
#include "exception.hpp"
#include <cstdio>
namespace sjtu {
	//B+�������洢��ַ
	constexpr char BPTREE_ADDRESS[128] = "bptree_data.sjtu";
	template <class Key, class Value, class Compare = std::less<Key> >
	class BTree {
	private:
		// Your private members go here

//��Ŀ�ͷ����
		//����block��ͷ���洢�ÿ�Ļ�����Ϣ�����Ƿ�ΪҶ�ӡ������ж������ݵȣ���index��leaf������һ��head
		class Block_Head {
		public:
			//�洢����(0��ͨ 1Ҷ��)
			bool block_type = false;
			//����
			off_t _size = 0;
			//���λ��
			off_t _pos = 0;
			//�����
			off_t _parent = 0;
			//��һ�����
			off_t _last = 0;
			//��һ�����
			off_t _next = 0;
		};

		//����BPtree����Ҫ��Ϣ��

				//B+���ڵ���С
		constexpr static off_t BLOCK_SIZE = 4096;
		//�����ݿ����ݿ��С
		constexpr static off_t INIT_SIZE = sizeof(Block_Head);
		//Key���͵Ĵ�С
		constexpr static off_t KEY_SIZE = sizeof(Key);
		//Value���͵Ĵ�С
		constexpr static off_t VALUE_SIZE = sizeof(Value);
		//���ܹ��洢�����ĸ���
		constexpr static off_t BLOCK_KEY_NUM = (BLOCK_SIZE - INIT_SIZE) / sizeof(Normal_Data_Node) - 1;
		//���ܹ�������ݵĸ���
		constexpr static off_t BLOCK_PAIR_NUM = (BLOCK_SIZE - INIT_SIZE) / (KEY_SIZE + VALUE_SIZE) - 1;

		//���岿�֣�����������
				//����index�Ĺ��ɳɷ�
		struct Normal_Data_Node {
			off_t _child = 0;
			Key _key;
		};
		//leaf�Ĺ��ɳɷ־���pair


		//����index������ɷ�
		class Normal_Data {
		public:
			Normal_Data_Node val[BLOCK_KEY_NUM];
		};
		//����leaf�����岿��
		class Leaf_Data {
		public:
			pair<Key, Value> val[BLOCK_PAIR_NUM];
		};

		//�ļ�����
				//B+���ļ�ͷ
		class File_Head {
		public:
			//�洢BLOCKռ�õĿռ�
			off_t block_cnt = 1;
			//�洢���ڵ��λ��
			off_t root_pos = 0;
			//�洢���ݿ�ͷ
			off_t data_block_head = 0;
			//�洢���ݿ�β
			off_t data_block_rear = 0;
			//�洢��С
			off_t _size = 0;
		};

		//������Ա������

				//�ļ�ͷ
		File_Head tree_data;

		//�ļ�ָ��
		static FILE* fp;


		//���ú������֣�
		//����bptree�Ĺؼ���д����Ҫ�����̴���ģ����������Ǥ�
		//�B���äƤͣ�����
				//���ڴ��ȡ���Ƚ�ָ���ƶ���Ŀ�겿�֣��ٽ��й̶����ȶ�ȡ
		template <class MEM_TYPE>
		static void mem_read(MEM_TYPE buff, off_t buff_size, off_t pos)
		{
			fseek(fp, long(buff_size * pos), SEEK_SET);
			fread(buff, buff_size, 1, fp);
		}

		//���ڴ�д�룺�Ƚ�ָ���ƶ���ָ��λ�ã���д�룬����fflush��֤����ȫ�������ļ�����ֹ�ļ��رն�����
		template <class MEM_TYPE>
		static void mem_write(MEM_TYPE buff, off_t buff_size, off_t pos)
		{
			fseek(fp, long(buff_size * pos), SEEK_SET);
			fwrite(buff, buff_size, 1, fp);
			fflush(fp);
		}

		//�������������Ϣ�����ڴ浽���
		void write_tree_data()
		{
			fseek(fp, 0, SEEK_SET);
			char buff[BLOCK_SIZE] = { 0 };
			memcpy(buff, &tree_data, sizeof(tree_data));
			mem_write(buff, BLOCK_SIZE, 0);
		}

		//�������ڴ棺���ҵ��Ŀ��õ�ַ��0 memset
		off_t memory_allocation()
		{
			++tree_data.block_cnt;
			write_tree_data();
			char buff[BLOCK_SIZE] = { 0 };
			mem_write(buff, BLOCK_SIZE, tree_data.block_cnt - 1);
			return tree_data.block_cnt - 1;
		}

		//�����µ�������㣺
		//����header��index�������г�ʼ�������ظõ�ַ
		off_t create_normal_node(off_t _parent)
		{
			auto node_pos = memory_allocation();
			Block_Head temp;
			Normal_Data normal_data;

			temp.block_type = false;
			temp._parent = _parent;
			temp._pos = node_pos;
			temp._size = 0;

			write_block(&temp, &normal_data, node_pos);
			return node_pos;
		}

		//�����µ�Ҷ�ӽ�㣺
		//����header��leaf�������г�ʼ�������ظõ�ַ
		off_t create_leaf_node(off_t _parent, off_t _last, off_t _next)
		{
			auto node_pos = memory_allocation();
			Block_Head temp;
			Leaf_Data leaf_data;

			temp.block_type = true;
			temp._parent = _parent;
			temp._pos = node_pos;
			temp._last = _last;
			temp._next = _next;
			temp._size = 0;

			write_block(&temp, &leaf_data, node_pos);
			return node_pos;
		}

		//�����ڵ������������
		//�ڲ�����С������£���originλ�õ�key�Լ�֮��Ķ�������һ���ٰ�new_pos��_child��_key�ֱ�ŵ�origin����֮���λ�� 
		void insert_new_index(Block_Head& parent_info, Normal_Data& parent_data, off_t origin, off_t new_pos, const Key& new_index)
		{
			++parent_info._size;
			auto p = parent_info._size - 2;
			for (; parent_data.val[p]._child != origin; --p)
			{
				parent_data.val[p + 1] = parent_data.val[p];
			}

			parent_data.val[p + 1]._key = parent_data.val[p]._key;
			parent_data.val[p]._key = new_index;
			parent_data.val[p + 1]._child = new_pos;
		}

		//д��ڵ���Ϣ
		//�����ڴ洴���õ�head��index/leaf�����ļ���
		template <class DATA_TYPE>
		static void write_block(Block_Head* _info, DATA_TYPE* _data, off_t _pos)
		{
			char buff[BLOCK_SIZE] = { 0 };
			memcpy(buff, _info, sizeof(Block_Head));
			memcpy(buff + INIT_SIZE, _data, sizeof(DATA_TYPE));
			mem_write(buff, BLOCK_SIZE, _pos);
		}

		//��ȡ�����Ϣ
		//��_posλ���ϵ���Ϣ����buff,�ٷֱ�ת�Ƶ�head��data��
		template <class DATA_TYPE>
		static void read_block(Block_Head* _info, DATA_TYPE* _data, off_t _pos)
		{
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, _pos);
			memcpy(_info, buff, sizeof(Block_Head));
			memcpy(_data, buff + INIT_SIZE, sizeof(DATA_TYPE));
		}

		//����Ҷ�ӽ��
		//
		Key split_leaf_node(off_t pos, Block_Head& origin_info, Leaf_Data& origin_data)
		{
			//���ѵ�ǰ�������и���


			//�´����ڵ㣺��Ǹ���
			off_t parent_pos;
			Block_Head parent_info;
			Normal_Data parent_data;


			//�ж��Ƿ�Ϊ�����
			if (pos == tree_data.root_pos) {
				//�������ڵ���Ϊ����
				auto root_pos = create_normal_node(0);
				tree_data.root_pos = root_pos;
				write_tree_data();
				read_block(&parent_info, &parent_data, root_pos);
				origin_info._parent = root_pos;
				++parent_info._size;
				parent_data.val[0]._child = pos;
				parent_pos = root_pos;
			}
			//����ֱ���Ҹ���
			else
			{
				read_block(&parent_info, &parent_data, origin_info._parent);
				parent_pos = parent_info._pos;
			}

			//��鸸���Ƿ��������Ƿ���Ҫ���Ѹ���
			if (check_parent(origin_info)) {
				parent_pos = origin_info._parent;
				read_block(&parent_info, &parent_data, parent_pos);
			}

			//����һ���µ��ӽ�㣬��ŷ��ѵĺ�벿��
			auto new_pos = create_leaf_node(parent_pos, pos, origin_info._next);
			Block_Head new_info;
			Leaf_Data new_data;
			read_block(&new_info, &new_data, new_pos);

			//�޸ĺ�̽���ǰ��
			auto temp_pos = origin_info._next;
			Block_Head temp_info;
			Leaf_Data temp_data;
			read_block(&temp_info, &temp_data, temp_pos);
			temp_info._last = new_pos;
			write_block(&temp_info, &temp_data, temp_pos);
			//ԭ�ڵ�ĺ���Ϊ�½ڵ�
			origin_info._next = new_pos;

			//�ƶ����ݵ�λ��

			off_t mid_pos = origin_info._size >> 1;
			for (off_t p = mid_pos, i = 0; p < origin_info._size; ++p, ++i) {
				new_data.val[i].first = origin_data.val[p].first;
				new_data.val[i].second = origin_data.val[p].second;
				++new_info._size;
			}
			origin_info._size = mid_pos;
			//��new_pos����parent_index����
			insert_new_index(parent_info, parent_data, pos, new_pos, origin_data.val[mid_pos].first);

			//write in
			//����parent��origin��new_pos
			write_block(&origin_info, &origin_data, pos);
			write_block(&new_info, &new_data, new_pos);
			write_block(&parent_info, &parent_data, parent_pos);

			return new_data.val[0].first;
		}

		//���Ѹ��ף������µĸ��ף�
		bool check_parent(Block_Head& child_info)
		{
			//��������
			off_t parent_pos, origin_pos = child_info._parent;
			Block_Head parent_info, origin_info;
			Normal_Data parent_data, origin_data;
			read_block(&origin_info, &origin_data, origin_pos);
			if (origin_info._size < BLOCK_KEY_NUM)
				return false;

			//�ж��Ƿ�Ϊ�����
			if (origin_pos == tree_data.root_pos) {
				//�������ڵ�
				auto root_pos = create_normal_node(0);
				tree_data.root_pos = root_pos;
				write_tree_data();
				read_block(&parent_info, &parent_data, root_pos);
				origin_info._parent = root_pos;
				++parent_info._size;
				parent_data.val[0]._child = origin_pos;
				parent_pos = root_pos;
			}
			else
			{
				read_block(&parent_info, &parent_data, origin_info._parent);
				parent_pos = parent_info._pos;
			}
			if (check_parent(origin_info))
			{
				parent_pos = origin_info._parent;
				read_block(&parent_info, &parent_data, parent_pos);
			}
			//����һ���µ��ӽ��
			auto new_pos = create_normal_node(parent_pos);
			Block_Head new_info;
			Normal_Data new_data;
			read_block(&new_info, &new_data, new_pos);

			//�ƶ����ݵ�λ��
			off_t mid_pos = origin_info._size >> 1;
			for (off_t p = mid_pos + 1, i = 0; p < origin_info._size; ++p, ++i) {
				if (origin_data.val[p]._child == child_info._pos) {
					child_info._parent = new_pos;
				}
				std::swap(new_data.val[i], origin_data.val[p]);
				++new_info._size;
			}
			origin_info._size = mid_pos + 1;
			insert_new_index(parent_info, parent_data, origin_pos, new_pos, origin_data.val[mid_pos]._key);

			//write in
			write_block(&origin_info, &origin_data, origin_pos);
			write_block(&new_info, &new_data, new_pos);
			write_block(&parent_info, &parent_data, parent_pos);
			return true;
		}

		//�޸�LCA������(ƽ��Ҷ��ʱ)
		void change_index(off_t l_parent, off_t l_child, const Key& new_key) {
			//��ȡ����
			Block_Head parent_info;
			Normal_Data parent_data;
			read_block(&parent_info, &parent_data, l_parent);
			if (parent_data.val[parent_info._size - 1]._child == l_child) {
				change_index(parent_info._parent, l_parent, new_key);
				return;
			}
			for (off_t cur_pos = parent_info._size - 2;; --cur_pos) {
				if (parent_data.val[cur_pos]._child == l_child) {
					parent_data.val[cur_pos]._key = new_key;
					break;
				}
			}
			write_block(&parent_info, &parent_data, l_parent);
		}

		//�ϲ�����
		void merge_normal(Block_Head& l_info, Normal_Data& l_data, Block_Head& r_info, Normal_Data& r_data) {
			for (off_t p = l_info._size, i = 0; i < r_info._size; ++p, ++i) {
				l_data.val[p] = r_data.val[i];
			}
			l_data.val[l_info._size - 1]._key = adjust_normal(r_info._parent, r_info._pos);
			l_info._size += r_info._size;
			write_block(&l_info, &l_data, l_info._pos);
		}

		//ƽ������
		void balance_normal(Block_Head& info, Normal_Data& normal_data) {
			if (info._size >= BLOCK_KEY_NUM / 2) {
				write_block(&info, &normal_data, info._pos);
				return;
			}
			//�ж��Ƿ��Ǹ�
			if (info._pos == tree_data.root_pos && info._size <= 1) {
				tree_data.root_pos = normal_data.val[0]._child;
				write_tree_data();
				return;
			}
			else if (info._pos == tree_data.root_pos) {
				write_block(&info, &normal_data, info._pos);
				return;
			}
			//��ȡ�ֵ�
			Block_Head parent_info, brother_info;
			Normal_Data parent_data, brother_data;
			read_block(&parent_info, &parent_data, info._parent);
			off_t value_pos;
			for (value_pos = 0; parent_data.val[value_pos]._child != info._pos; ++value_pos);
			if (value_pos > 0) {
				read_block(&brother_info, &brother_data, parent_data.val[value_pos - 1]._child);
				brother_info._parent = info._parent;
				if (brother_info._size > BLOCK_KEY_NUM / 2) {
					for (off_t p = info._size; p > 0; --p) {
						normal_data.val[p] = normal_data.val[p - 1];
					}
					normal_data.val[0]._child = brother_data.val[brother_info._size - 1]._child;
					normal_data.val[0]._key = parent_data.val[value_pos - 1]._key;
					parent_data.val[value_pos - 1]._key = brother_data.val[brother_info._size - 2]._key;
					--brother_info._size;
					++info._size;
					write_block(&brother_info, &brother_data, brother_info._pos);
					write_block(&info, &normal_data, info._pos);
					write_block(&parent_info, &parent_data, parent_info._pos);
					return;
				}
				else {
					merge_normal(brother_info, brother_data, info, normal_data);
					return;
				}
			}
			if (value_pos < parent_info._size - 1) {
				read_block(&brother_info, &brother_data, parent_data.val[value_pos + 1]._child);
				brother_info._parent = info._parent;
				if (brother_info._size > BLOCK_KEY_NUM / 2) {
					normal_data.val[info._size]._child = brother_data.val[0]._child;
					normal_data.val[info._size - 1]._key = parent_data.val[value_pos]._key;
					parent_data.val[value_pos]._key = brother_data.val[0]._key;
					for (off_t p = 1; p < brother_info._size; ++p) {
						brother_data.val[p - 1] = brother_data.val[p];
					}
					--brother_info._size;
					++info._size;
					write_block(&brother_info, &brother_data, brother_info._pos);
					write_block(&info, &normal_data, info._pos);
					write_block(&parent_info, &parent_data, parent_info._pos);
					return;
				}
				else {
					merge_normal(info, normal_data, brother_info, brother_data);
					return;
				}
			}
		}

		//��������(���عؼ���)
		Key adjust_normal(off_t pos, off_t removed_child) {
			Block_Head info;
			Normal_Data normal_data;
			read_block(&info, &normal_data, pos);
			off_t cur_pos;
			for (cur_pos = 0; normal_data.val[cur_pos]._child != removed_child; ++cur_pos);
			Key ans = normal_data.val[cur_pos - 1]._key;
			normal_data.val[cur_pos - 1]._key = normal_data.val[cur_pos]._key;
			for (; cur_pos < info._size - 1; ++cur_pos) {
				normal_data.val[cur_pos] = normal_data.val[cur_pos + 1];
			}
			--info._size;
			balance_normal(info, normal_data);
			return ans;
		}

		//�ϲ�Ҷ��
		void merge_leaf(Block_Head& l_info, Leaf_Data& l_data, Block_Head& r_info, Leaf_Data& r_data) {
			for (off_t p = l_info._size, i = 0; i < r_info._size; ++p, ++i) {
				l_data.val[p].first = r_data.val[i].first;
				l_data.val[p].second = r_data.val[i].second;
			}
			l_info._size += r_info._size;
			adjust_normal(r_info._parent, r_info._pos);
			//�޸�����
			l_info._next = r_info._next;
			Block_Head temp_info;
			Leaf_Data temp_data;
			read_block(&temp_info, &temp_data, r_info._next);
			temp_info._last = l_info._pos;
			write_block(&temp_info, &temp_data, temp_info._pos);
			write_block(&l_info, &l_data, l_info._pos);
		}

		//ƽ��Ҷ��
		void balance_leaf(Block_Head& leaf_info, Leaf_Data& leaf_data) {
			if (leaf_info._size >= BLOCK_PAIR_NUM / 2) {
				write_block(&leaf_info, &leaf_data, leaf_info._pos);
				return;
			}
			else if (leaf_info._pos == tree_data.root_pos) {
				if (leaf_info._size == 0) {
					Block_Head temp_info;
					Leaf_Data temp_data;
					read_block(&temp_info, &temp_data, tree_data.data_block_head);
					temp_info._next = tree_data.data_block_rear;
					write_block(&temp_info, &temp_data, tree_data.data_block_head);
					read_block(&temp_info, &temp_data, tree_data.data_block_rear);
					temp_info._last = tree_data.data_block_head;
					write_block(&temp_info, &temp_data, tree_data.data_block_rear);
					return;
				}
				write_block(&leaf_info, &leaf_data, leaf_info._pos);
				return;
			}
			//�����ֵܽ��
			Block_Head brother_info, parent_info;
			Leaf_Data brother_data;
			Normal_Data parent_data;

			read_block(&parent_info, &parent_data, leaf_info._parent);
			off_t node_pos = 0;
			for (; node_pos < parent_info._size; ++node_pos) {
				if (parent_data.val[node_pos]._child == leaf_info._pos)
					break;
			}

			//���ֵ�
			if (node_pos > 0) {
				read_block(&brother_info, &brother_data, leaf_info._last);
				brother_info._parent = leaf_info._parent;
				if (brother_info._size > BLOCK_PAIR_NUM / 2) {
					for (off_t p = leaf_info._size; p > 0; --p) {
						leaf_data.val[p].first = leaf_data.val[p - 1].first;
						leaf_data.val[p].second = leaf_data.val[p - 1].second;
					}
					leaf_data.val[0].first = brother_data.val[brother_info._size - 1].first;
					leaf_data.val[0].second = brother_data.val[brother_info._size - 1].second;
					--brother_info._size;
					++leaf_info._size;
					change_index(brother_info._parent, brother_info._pos, leaf_data.val[0].first);
					write_block(&brother_info, &brother_data, brother_info._pos);
					write_block(&leaf_info, &leaf_data, leaf_info._pos);
					return;
				}
				else {
					merge_leaf(brother_info, brother_data, leaf_info, leaf_data);
					//write_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
			}
			//���ֵ�
			if (node_pos < parent_info._size - 1) {
				read_block(&brother_info, &brother_data, leaf_info._next);
				brother_info._parent = leaf_info._parent;
				if (brother_info._size > BLOCK_PAIR_NUM / 2) {
					leaf_data.val[leaf_info._size].first = brother_data.val[0].first;
					leaf_data.val[leaf_info._size].second = brother_data.val[0].second;
					for (off_t p = 1; p < brother_info._size; ++p) {
						brother_data.val[p - 1].first = brother_data.val[p].first;
						brother_data.val[p - 1].second = brother_data.val[p].second;
					}
					++leaf_info._size;
					--brother_info._size;
					change_index(leaf_info._parent, leaf_info._pos, brother_data.val[0].first);
					write_block(&leaf_info, &leaf_data, leaf_info._pos);
					write_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
				else {
					merge_leaf(leaf_info, leaf_data, brother_info, brother_data);
					//write_block(&brother_info, &brother_data, brother_info._pos);
					return;
				}
			}
		}

		//�����ļ�
		void check_file() {
			if (!fp) {
				//�����µ���
				fp = fopen(BPTREE_ADDRESS, "wb+");
				write_tree_data();

				auto node_head = tree_data.block_cnt,
					node_rear = tree_data.block_cnt + 1;

				tree_data.data_block_head = node_head;
				tree_data.data_block_rear = node_rear;

				create_leaf_node(0, 0, node_rear);
				create_leaf_node(0, node_head, 0);

				return;
			}
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, 0);
			memcpy(&tree_data, buff, sizeof(tree_data));
		}
	public:
		typedef pair<const Key, Value> value_type;

		class const_iterator;
		class iterator {
			friend class sjtu::BTree<Key, Value, Compare>::const_iterator;
			friend iterator sjtu::BTree<Key, Value, Compare>::begin();
			friend iterator sjtu::BTree<Key, Value, Compare>::end();
			friend iterator sjtu::BTree<Key, Value, Compare>::find(const Key&);
			friend pair<iterator, OperationResult> sjtu::BTree<Key, Value, Compare>::insert(const Key&, const Value&);
		private:
			// Your private members go here
			//�洢��ǰ
			BTree* cur_bptree = nullptr;
			//�洢��ǰ��Ļ�����Ϣ
			Block_Head block_info;
			//�洢��ǰָ���Ԫ��λ��
			off_t cur_pos = 0;

		public:
			bool modify(const Value& value) {
				Block_Head info;
				Leaf_Data leaf_data;
				read_block(&info, &leaf_data, block_info._pos);
				leaf_data.val[cur_pos].second = value;
				write_block(&info, &leaf_data, block_info._pos);
				return true;
			}
			iterator() {
				// TODO Default Constructor
			}
			iterator(const iterator& other) {
				// TODO Copy Constructor
				cur_bptree = other.cur_bptree;
				block_info = other.block_info;
				cur_pos = other.cur_pos;
			}
			// Return a new iterator which points to the n-next elements
			iterator operator++(int) {
				// Todo iterator++
				auto temp = *this;
				++cur_pos;
				if (cur_pos >= block_info._size) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._next);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = 0;
				}
				return temp;
			}
			iterator& operator++() {
				// Todo ++iterator
				++cur_pos;
				if (cur_pos >= block_info._size) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._next);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = 0;
				}
				return *this;
			}
			iterator operator--(int) {
				// Todo iterator--
				auto temp = *this;
				if (cur_pos == 0) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._last);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = block_info._size - 1;
				}
				else
					--cur_pos;
				return temp;
			}
			iterator& operator--() {
				// Todo --iterator
				if (cur_pos == 0) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._last);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = block_info._size - 1;
				}
				else
					--cur_pos;

				return *this;
			}
			// Overloaded of operator '==' and '!='
			// Check whether the iterators are same
			value_type operator*() const {
				// Todo operator*, return the <K,V> of iterator
				if (cur_pos >= block_info._size)
					throw invalid_iterator();
				char buff[BLOCK_SIZE] = { 0 };
				mem_read(buff, BLOCK_SIZE, block_info._pos);
				Leaf_Data leaf_data;
				memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
				value_type result(leaf_data.val[cur_pos].first, leaf_data.val[cur_pos].second);
				return result;
			}
			bool operator==(const iterator& rhs) const {
				// Todo operator ==
				return cur_bptree == rhs.cur_bptree
					&& block_info._pos == rhs.block_info._pos
					&& cur_pos == rhs.cur_pos;
			}
			bool operator==(const const_iterator& rhs) const {
				// Todo operator ==
				return block_info._pos == rhs.block_info._pos
					&& cur_pos == rhs.cur_pos;
			}
			bool operator!=(const iterator& rhs) const {
				// Todo operator !=
				return cur_bptree != rhs.cur_bptree
					|| block_info._pos != rhs.block_info._pos
					|| cur_pos != rhs.cur_pos;
			}
			bool operator!=(const const_iterator& rhs) const {
				// Todo operator !=
				return block_info._pos != rhs.block_info._pos
					|| cur_pos != rhs.cur_pos;
			}
		};
		class const_iterator {
			// it should has similar member method as iterator.
			//  and it should be able to construct from an iterator.
			friend class sjtu::BTree<Key, Value, Compare>::iterator;
			friend const_iterator sjtu::BTree<Key, Value, Compare>::cbegin() const;
			friend const_iterator sjtu::BTree<Key, Value, Compare>::cend() const;
			friend const_iterator sjtu::BTree<Key, Value, Compare>::find(const Key&) const;
		private:
			// Your private members go here
			//�洢��ǰ��Ļ�����Ϣ
			Block_Head block_info;
			//�洢��ǰָ���Ԫ��λ��
			off_t cur_pos = 0;
		public:
			const_iterator() {
				// TODO
			}
			const_iterator(const const_iterator& other) {
				// TODO
				block_info = other.block_info;
				cur_pos = other.cur_pos;
			}
			const_iterator(const iterator& other) {
				// TODO
				block_info = other.block_info;
				cur_pos = other.cur_pos;
			}
			// And other methods in iterator, please fill by yourself.
			// Return a new iterator which points to the n-next elements
			const_iterator operator++(int) {
				// Todo iterator++
				auto temp = *this;
				++cur_pos;
				if (cur_pos >= block_info._size) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._next);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = 0;
				}
				return temp;
			}
			const_iterator& operator++() {
				// Todo ++iterator
				++cur_pos;
				if (cur_pos >= block_info._size) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._next);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = 0;
				}
				return *this;
			}
			const_iterator operator--(int) {
				// Todo iterator--
				auto temp = *this;
				if (cur_pos == 0) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._last);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = block_info._size - 1;
				}
				else
					--cur_pos;
				return temp;
			}
			const_iterator& operator--() {
				// Todo --iterator
				if (cur_pos == 0) {
					char buff[BLOCK_SIZE] = { 0 };
					mem_read(buff, BLOCK_SIZE, block_info._last);
					memcpy(&block_info, buff, sizeof(block_info));
					cur_pos = block_info._size - 1;
				}
				else
					--cur_pos;

				return *this;
			}
			// Overloaded of operator '==' and '!='
			// Check whether the iterators are same
			value_type operator*() const {
				// Todo operator*, return the <K,V> of iterator
				if (cur_pos >= block_info._size)
					throw invalid_iterator();
				char buff[BLOCK_SIZE] = { 0 };
				mem_read(buff, BLOCK_SIZE, block_info._pos);
				Leaf_Data leaf_data;
				memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
				value_type result(leaf_data.val[cur_pos].first, leaf_data.val[cur_pos].second);
				return result;
			}
			bool operator==(const iterator& rhs) const {
				// Todo operator ==
				return block_info._pos == rhs.block_info._pos
					&& cur_pos == rhs.cur_pos;
			}
			bool operator==(const const_iterator& rhs) const {
				// Todo operator ==
				return block_info._pos == rhs.block_info._pos
					&& cur_pos == rhs.cur_pos;
			}
			bool operator!=(const iterator& rhs) const {
				// Todo operator !=
				return block_info._pos != rhs.block_info._pos
					|| cur_pos != rhs.cur_pos;
			}
			bool operator!=(const const_iterator& rhs) const {
				// Todo operator !=
				return block_info._pos != rhs.block_info._pos
					|| cur_pos != rhs.cur_pos;
			}
		};


		// Default Constructor and Copy Constructor
		BTree() {
			// Todo Default
			fp = fopen(BPTREE_ADDRESS, "rb+");
			if (!fp) {
				//�����µ���
				fp = fopen(BPTREE_ADDRESS, "wb+");
				write_tree_data();

				auto node_head = tree_data.block_cnt,
					node_rear = tree_data.block_cnt + 1;

				tree_data.data_block_head = node_head;
				tree_data.data_block_rear = node_rear;

				create_leaf_node(0, 0, node_rear);
				create_leaf_node(0, node_head, 0);

				return;
			}
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, 0);
			memcpy(&tree_data, buff, sizeof(tree_data));
		}
		BTree(const BTree& other) {
			fp = fopen(BPTREE_ADDRESS, "rb+");
			tree_data.block_cnt = other.tree_data.block_cnt;
			tree_data.data_block_head = other.tree_data.data_block_head;
			tree_data.data_block_rear = other.tree_data.data_block_rear;
			tree_data.root_pos = other.tree_data.root_pos;
			tree_data._size = other.tree_data._size;
		}
		BTree& operator=(const BTree& other) {
			// Todo Assignment
			fp = fopen(BPTREE_ADDRESS, "rb+");
			tree_data.block_cnt = other.tree_data.block_cnt;
			tree_data.data_block_head = other.tree_data.data_block_head;
			tree_data.data_block_rear = other.tree_data.data_block_rear;
			tree_data.root_pos = other.tree_data.root_pos;
			tree_data._size = other.tree_data._size;
			return *this;
		}
		~BTree() {

			fclose(fp);
		}
		// Insert: Insert certain Key-Value into the database
		// Return a pair, the first of the pair is the iterator point to the new
		// element, the second of the pair is Success if it is successfully inserted
		pair<iterator, OperationResult> insert(const Key& key, const Value& value)
		{
			check_file();
			if (empty())
			{
				auto root_pos = create_leaf_node(0, tree_data.data_block_head, tree_data.data_block_rear);

				Block_Head temp_info;
				Leaf_Data temp_data;

				//����head
				read_block(&temp_info, &temp_data, tree_data.data_block_head);
				temp_info._next = root_pos;
				write_block(&temp_info, &temp_data, tree_data.data_block_head);

				//����rear
				read_block(&temp_info, &temp_data, tree_data.data_block_rear);
				temp_info._last = root_pos;
				write_block(&temp_info, &temp_data, tree_data.data_block_rear);

				//����pair
				read_block(&temp_info, &temp_data, root_pos);
				++temp_info._size;
				temp_data.val[0].first = key;
				temp_data.val[0].second = value;
				write_block(&temp_info, &temp_data, root_pos);


				//����tree_data
				++tree_data._size;
				tree_data.root_pos = root_pos;
				write_tree_data();

				pair<iterator, OperationResult> result(begin(), Success);
				return result;
			}


			//������ȷ�Ľڵ�λ��
			char buff[BLOCK_SIZE] = { 0 };
			off_t cur_pos = tree_data.root_pos, cur_parent = 0;
			while (true) {
				mem_read(buff, BLOCK_SIZE, cur_pos);
				Block_Head temp;
				memcpy(&temp, buff, sizeof(temp));

				//�жϸ����Ƿ����
				if (cur_parent != temp._parent)
				{
					temp._parent = cur_parent;
					memcpy(buff, &temp, sizeof(temp));
					mem_write(buff, BLOCK_SIZE, cur_pos);
				}
				if (temp.block_type)
				{
					break;
				}

				Normal_Data normal_data;
				memcpy(&normal_data, buff + INIT_SIZE, sizeof(normal_data));
				off_t child_pos = temp._size - 1;
				for (; child_pos > 0; --child_pos) {
					if (!(normal_data.val[child_pos - 1]._key > key)) {
						break;
					}
				}
				cur_parent = cur_pos;
				cur_pos = normal_data.val[child_pos]._child;

			}

			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			Leaf_Data leaf_data;
			memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
			for (off_t value_pos = 0;; ++value_pos) {
				if (value_pos < info._size && (!(leaf_data.val[value_pos].first<key || leaf_data.val[value_pos].first>key))) {
					//throw runtime_error();
					return pair<iterator, OperationResult>(end(), Fail);
				}
				if (value_pos >= info._size || leaf_data.val[value_pos].first > key) {
					//�ڴ˽��֮ǰ����
					if (info._size >= BLOCK_PAIR_NUM) {
						auto cur_key = split_leaf_node(cur_pos, info, leaf_data);
						if (key > cur_key) {
							cur_pos = info._next;
							value_pos -= info._size;
							read_block(&info, &leaf_data, cur_pos);
						}
					}

					for (off_t p = info._size - 1; p >= value_pos; --p) {
						leaf_data.val[p + 1].first = leaf_data.val[p].first;
						leaf_data.val[p + 1].second = leaf_data.val[p].second;
						if (p == value_pos)
							break;
					}
					leaf_data.val[value_pos].first = key;
					leaf_data.val[value_pos].second = value;
					++info._size;
					write_block(&info, &leaf_data, cur_pos);
					iterator ans;
					ans.block_info = info;
					ans.cur_bptree = this;
					ans.cur_pos = value_pos;
					//�޸����Ļ�������
					++tree_data._size;
					write_tree_data();
					pair<iterator, OperationResult> to_return(ans, Success);
					return to_return;
				}
			}
			return pair<iterator, OperationResult>(end(), Fail);
		}
		// Erase: Erase the Key-Value
		// Return Success if it is successfully erased
		// Return Fail if the key doesn't exist in the database
		OperationResult erase(const Key& key) {
			check_file();
			// TODO erase function
			if (empty()) {
				return Fail;
			}
			//������ȷ�Ľڵ�λ��
			char buff[BLOCK_SIZE] = { 0 };
			off_t cur_pos = tree_data.root_pos, cur_parent = 0;
			while (true) {
				mem_read(buff, BLOCK_SIZE, cur_pos);
				Block_Head temp;
				memcpy(&temp, buff, sizeof(temp));
				//�жϸ����Ƿ����
				if (cur_parent != temp._parent)
				{
					temp._parent = cur_parent;
					memcpy(buff, &temp, sizeof(temp));
					mem_write(buff, BLOCK_SIZE, cur_pos);
				}
				if (temp.block_type)
				{
					break;
				}
				Normal_Data normal_data;
				memcpy(&normal_data, buff + INIT_SIZE, sizeof(normal_data));
				off_t child_pos = temp._size - 1;
				for (; child_pos > 0; --child_pos)
				{
					if (!(normal_data.val[child_pos - 1]._key > key))
					{
						break;
					}
				}
				cur_parent = cur_pos;
				cur_pos = normal_data.val[child_pos]._child;
			}

			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			Leaf_Data leaf_data;
			memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
			for (off_t value_pos = 0;; ++value_pos) {
				if (value_pos < info._size && (!(leaf_data.val[value_pos].first<key || leaf_data.val[value_pos].first>key))) {
					//ִ��ɾ������
					--info._size;
					for (off_t p = value_pos; p < info._size; ++p) {
						leaf_data.val[p].first = leaf_data.val[p + 1].first;
						leaf_data.val[p].second = leaf_data.val[p + 1].second;
					}
					balance_leaf(info, leaf_data);
					--tree_data._size;
					write_tree_data();
					return Success;
				}
				if (value_pos >= info._size || leaf_data.val[value_pos].first > key) {
					return Fail;
				}
			}
			return Fail;  // I can finish this part!!! 
		}
		iterator begin() {
			check_file();
			iterator result;
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, tree_data.data_block_head);
			Block_Head block_head;
			memcpy(&block_head, buff, sizeof(block_head));
			result.block_info = block_head;
			result.cur_bptree = this;
			result.cur_pos = 0;
			++result;
			return result;
		}
		const_iterator cbegin() const {
			const_iterator result;
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, tree_data.data_block_head);
			Block_Head block_head;
			memcpy(&block_head, buff, sizeof(block_head));
			result.block_info = block_head;
			result.cur_pos = 0;
			++result;
			return result;
		}
		// Return a iterator to the end(the next element after the last)
		iterator end() {
			check_file();
			iterator result;
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, tree_data.data_block_rear);
			Block_Head block_head;
			memcpy(&block_head, buff, sizeof(block_head));
			result.block_info = block_head;
			result.cur_bptree = this;
			result.cur_pos = 0;
			return result;
		}
		const_iterator cend() const {
			const_iterator result;
			char buff[BLOCK_SIZE] = { 0 };
			mem_read(buff, BLOCK_SIZE, tree_data.data_block_rear);
			Block_Head block_head;
			memcpy(&block_head, buff, sizeof(block_head));
			result.block_info = block_head;
			result.cur_pos = 0;
			return result;
		}


		bool empty() const {
			if (!fp)
				return true;
			return tree_data._size == 0;
		}


		off_t size() const {
			if (!fp)
				return 0;
			return tree_data._size;
		}


		void clear() {
			if (!fp)
				return;
			//δ��ɵĹ���(��֪���к��ã�
			remove(BPTREE_ADDRESS);
			File_Head new_file_head;
			tree_data = new_file_head;
			fp = nullptr;
		}
		//�������



		Value at(const Key& key) {
			if (empty()) {
				throw container_is_empty();
			}
			//������ȷ�Ľڵ�λ��
			char buff[BLOCK_SIZE] = { 0 };
			off_t cur_pos = tree_data.root_pos, cur_parent = 0;
			while (true)
			{
				mem_read(buff, BLOCK_SIZE, cur_pos);
				Block_Head temp;
				memcpy(&temp, buff, sizeof(temp));


				//�жϸ����Ƿ����
				if (cur_parent != temp._parent) {
					temp._parent = cur_parent;
					memcpy(buff, &temp, sizeof(temp));
					mem_write(buff, BLOCK_SIZE, cur_pos);
				}
				//�ҵ�Ҷ�ӽڵ�
				if (temp.block_type) {
					break;
				}

				Normal_Data normal_data;
				memcpy(&normal_data, buff + INIT_SIZE, sizeof(normal_data));
				//�����ң�ֱ���ҵ�����С��key����������Ŀ��ڵ�

				off_t child_pos = temp._size - 1;
				for (; child_pos > 0; --child_pos) {
					if (!(normal_data.val[child_pos - 1]._key > key)) {
						break;
					}
				}
				cur_pos = normal_data.val[child_pos]._child;

			}

			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			Leaf_Data leaf_data;
			memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
			for (off_t value_pos = 0;; ++value_pos) {
				if (value_pos < info._size && (!(leaf_data.val[value_pos].first<key || leaf_data.val[value_pos].first>key))) {
					return leaf_data.val[value_pos].second;
				}
				if (value_pos >= info._size || leaf_data.val[value_pos].first > key) {
					throw index_out_of_bound();
				}
			}
		}
		/**
		 * Returns the number of elements with key
		 *   that compares equivalent to the specified argument,
		 * The default method of check the equivalence is !(a < b || b > a)
		 */
		off_t count(const Key& key) const
		{
			return find(key) == cend() ? 0 : 1;
		}
		/**
		 * Finds an element with key equivalent to key.
		 * key value of the element to search for.
		 * Iterator to an element with key equivalent to key.
		 *   If no such element is found, past-the-end (see end()) iterator is
		 * returned.
		 */
		iterator find(const Key& key) {
			if (empty()) {
				return end();
			}
			//������ȷ�Ľڵ�λ��
			char buff[BLOCK_SIZE] = { 0 };
			off_t cur_pos = tree_data.root_pos, cur_parent = 0;
			while (true) {
				mem_read(buff, BLOCK_SIZE, cur_pos);
				Block_Head temp;
				memcpy(&temp, buff, sizeof(temp));
				//�жϸ����Ƿ����
				if (cur_parent != temp._parent) {
					temp._parent = cur_parent;
					memcpy(buff, &temp, sizeof(temp));
					mem_write(buff, BLOCK_SIZE, cur_pos);
				}
				if (temp.block_type) {
					break;
				}
				Normal_Data normal_data;
				memcpy(&normal_data, buff + INIT_SIZE, sizeof(normal_data));
				off_t child_pos = temp._size - 1;
				for (; child_pos > 0; --child_pos) {
					if (!(normal_data.val[child_pos - 1]._key > key)) {
						break;
					}
				}
				cur_pos = normal_data.val[child_pos]._child;
			}
			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			sizeof(Normal_Data);
			Leaf_Data leaf_data;
			memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
			for (off_t value_pos = 0;; ++value_pos) {
				if (value_pos < info._size && (!(leaf_data.val[value_pos].first<key || leaf_data.val[value_pos].first>key))) {
					iterator result;
					result.cur_bptree = this;
					result.block_info = info;
					result.cur_pos = value_pos;
					return result;
				}
				if (value_pos >= info._size || leaf_data.val[value_pos].first > key) {
					return end();
				}
			}
			return end();
		}
		const_iterator find(const Key& key) const
		{
			if (empty()) {
				return cend();
			}
			//������ȷ�Ľڵ�λ��
			char buff[BLOCK_SIZE] = { 0 };
			off_t cur_pos = tree_data.root_pos, cur_parent = 0;
			while (true) {
				mem_read(buff, BLOCK_SIZE, cur_pos);
				Block_Head temp;
				memcpy(&temp, buff, sizeof(temp));
				//�жϸ����Ƿ����
				if (cur_parent != temp._parent) {
					temp._parent = cur_parent;
					memcpy(buff, &temp, sizeof(temp));
					mem_write(buff, BLOCK_SIZE, cur_pos);
				}
				if (temp.block_type) {
					break;
				}
				Normal_Data normal_data;
				memcpy(&normal_data, buff + INIT_SIZE, sizeof(normal_data));
				off_t child_pos = temp._size - 1;
				for (; child_pos > 0; --child_pos) {
					if (!(normal_data.val[child_pos - 1]._key > key)) {
						break;
					}
				}
				cur_pos = normal_data.val[child_pos]._child;
			}
			Block_Head info;
			memcpy(&info, buff, sizeof(info));
			Leaf_Data leaf_data;
			memcpy(&leaf_data, buff + INIT_SIZE, sizeof(leaf_data));
			for (off_t value_pos = 0;; ++value_pos) {
				if (value_pos < info._size && (!(leaf_data.val[value_pos].first<key || leaf_data.val[value_pos].first>key))) {
					const_iterator result;
					result.block_info = info;
					result.cur_pos = value_pos;
					return result;
				}
				if (value_pos >= info._size || leaf_data.val[value_pos].first > key) {
					return cend();
				}
			}
			return cend();
		}
	};

	template <typename Key, typename Value, typename Compare> FILE* BTree<Key, Value, Compare>::fp = nullptr;
}