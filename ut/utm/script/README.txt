1. ��� ��� :

mig.sh username/passwd {out|in}
	out : �ش� ������ ����� ���� ��� ���̺��� export
	in  : ����� ���� ���ϵ��� �̿��Ͽ� �����͸� import
 
2. ���� �� �����Ǵ� ���ϵ�
 'username'_mig_list.txt     : �ش� ������ ���̱׷��̼� ��� ���̺��� �̸���.
 'username'_'tablename'.desc : �ش� ���̺� ���� ���� 
 'username'_'tablename'.sql  : �ش� ���̺� ���� ��ũ��Ʈ 
 'username'_'tablename'.fmt  : �ش� ���̺� iloader ������
 'username'_'tablename'.dat  : �ش� ���̺� iloader ������ ���� 
 'username'_'tablename'.olog : �ش� ���̺� iloader export �α����� - out �ÿ��� ���� 
 'username'_'tablename'.obad : �ش� ���̺� iloader export ������� - out �ÿ��� ���� 
 'username'_'tablename'.ilog : �ش� ���̺� iloader import �α����� - in �ÿ��� ���� 
 'username'_'tablename'.ibad : �ش� ���̺� iloader import ������� - in �ÿ��� ����
 
�̷��� ���ϵ��� �����Ų ���͸��� �ٷ� ����Ƿ� �۾��� �ӽ� ���͸��� ���� �����ϴ� ���� ���Ŀ� �����ϴµ� ���մϴ�.
 
3. functions in main

        in)
                makeTables : ���̺���� ��ũ��Ʈ�� �̿��Ͽ� ������ ���̺��� ����.
                dataImport : dat, fmt file�� �̿��Ͽ� data import.
        ;;
        out)
                getUserID
                getTableName
                makeTableDesc    : �ش� ���̺��� ���� ���� (.desc)�� ����
                #makeTableScript : ���̺� ���� ��ũ��Ʈ�� ����
                makeFormFile     : �������� ����
                dataExport       : .fmt�� ���̺� �̸��� �̿��Ͽ� export
        ;;
        *)
                usage
        ;;
esac
 
���� ���� ���� �ϵ��� �Ǿ� �ֽ��ϴ�.
��Ȳ�� ���� �ʿ���� ������ �ּ����� ���� ����ϼŵ� �˴ϴ�.

