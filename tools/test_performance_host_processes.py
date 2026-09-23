import unittest
from performance_host_processes import differences


class ProcessCpuTests(unittest.TestCase):
    def test_cpu_delta_can_exceed_one_core(self):
        old = {'rows': {(12, 1): dict(cpu=2, qpc=10, name='game')}}
        new = {'rows': {(12, 1): dict(cpu=5, qpc=12, name='game')}}
        row = differences(old, new, 12)[0]
        self.assertEqual(row['cpu_ms'], 3000)
        self.assertEqual(row['cores_used'], 1.5)

    def test_pid_reuse_and_counter_reset_are_not_cpu(self):
        old = {'rows': {(12, 1): dict(cpu=2, qpc=10, name='game')}}
        for key, cpu, qpc in [((12, 2), 20, 12), ((12, 1), 1, 12), ((12, 1), 3, 10)]:
            self.assertEqual(differences(old, {'rows': {key: dict(cpu=cpu, qpc=qpc, name='new')}}, 12), [])

    def test_top_ten_always_retains_target(self):
        old = {'rows': {(i, 1): dict(cpu=0, qpc=10, name=str(i)) for i in range(1, 15)}}
        new = {'rows': {(i, 1): dict(cpu=i, qpc=12, name=str(i)) for i in range(1, 15)}}
        rows = differences(old, new, 1)
        self.assertEqual(len(rows), 11)
        self.assertEqual(rows[0]['pid'], 14)
        self.assertEqual(rows[-1]['pid'], 1)


if __name__ == '__main__':
    unittest.main()
