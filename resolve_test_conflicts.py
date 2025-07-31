#!/usr/bin/env python3
import re

# Fix rpc_net.py
with open('test/functional/rpc_net.py', 'r') as f:
    content = f.read()

# Replace import conflict - keep Dash's imports
content = re.sub(r'<<<<<<< HEAD\nfrom test_framework\.messages import \(\n    MAX_PROTOCOL_MESSAGE_LENGTH,\n    NODE_NETWORK,\n=======\nfrom test_framework\.p2p import \(\n    P2PInterface,\n    P2P_SERVICES,\n>>>>>>> a945f09fa6.*?\)', 
                'from test_framework.messages import (\n    MAX_PROTOCOL_MESSAGE_LENGTH,\n    NODE_NETWORK,\n)', 
                content, flags=re.DOTALL)

# Replace class conflict - keep DashTestFramework
content = re.sub(r'<<<<<<< HEAD\nclass NetTest\(DashTestFramework\):\n=======\ndef seed_addrman.*?>>>>>>> a945f09fa6.*?\n', 
                'def seed_addrman(node):\n    """ Populate the addrman with addresses from different networks.\n    Here 2 ipv4, 2 ipv6, 1 cjdns, 2 onion and 1 i2p addresses are added.\n    """\n    node.addpeeraddress(address="192.168.5.1", port=8333)\n    node.addpeeraddress(address="192.168.5.2", port=9999)\n    node.addpeeraddress(address="[1233:3432:2434:2343:3234:2345:6546:4534]", port=8333)\n    node.addpeeraddress(address="[1233:3432:2434:2343:3234:2345:6546:4535]", port=9999)\n    node.addpeeraddress(address="[fc00:1:2:3:4:5:6:7]", port=8333)\n    node.addpeeraddress(address="w6cjzwxuhanpm536vqp4dbwyqcw44cw36fgq3khho7e7b52xgbvoikwd.onion", port=8333)\n    node.addpeeraddress(address="pg6mmjiyjmcrsslvykfwnntlaru7p5svn6y2ymmju6nubxndf4pscryd.onion", port=8333)\n    node.addpeeraddress(address="euklmqm5i5eeitu7dnvhb7zg4xmysn7jw2lzxwbmuojg2sydugkcq.b32.i2p", port=0)\n\n\nclass NetTest(DashTestFramework):\n', 
                content, flags=re.DOTALL)

# Add the test_getaddrmaninfo method after the last test
# Find where to insert it
last_test_match = re.search(r'(def test_.*?\n(?:.*?\n)*?)(\s*def run_test)', content, re.DOTALL)
if last_test_match:
    # Remove any remaining conflict markers first
    content = re.sub(r'<<<<<<< HEAD\n=======\n.*?>>>>>>> a945f09fa6.*?\n', '', content, flags=re.DOTALL)
    
    # Now add the new test method
    insertion_point = last_test_match.end(1)
    new_test = '''
    def test_getaddrmaninfo(self):
        self.log.info("Test getaddrmaninfo")
        self.restart_node(1, extra_args=["-cjdnsreachable", "-test=addrman"], clear_addrman=True)
        
        self.log.debug("Test that getaddrmaninfo is a hidden RPC")
        assert "getaddrmaninfo" not in self.nodes[1].help()
        
        # Populate the addrman
        seed_addrman(self.nodes[1])
        
        # Test getaddrmaninfo
        res = self.nodes[1].getaddrmaninfo()
        assert_equal(res["ipv4"]["new"], 2)
        assert_equal(res["ipv4"]["tried"], 0)
        assert_equal(res["ipv4"]["total"], 2)
        assert_equal(res["ipv6"]["new"], 2)
        assert_equal(res["ipv6"]["tried"], 0)
        assert_equal(res["ipv6"]["total"], 2)
        assert_equal(res["onion"]["new"], 2)
        assert_equal(res["onion"]["tried"], 0)
        assert_equal(res["onion"]["total"], 2)
        assert_equal(res["i2p"]["new"], 1)
        assert_equal(res["i2p"]["tried"], 0)
        assert_equal(res["i2p"]["total"], 1)
        assert_equal(res["cjdns"]["new"], 1)
        assert_equal(res["cjdns"]["tried"], 0)
        assert_equal(res["cjdns"]["total"], 1)
        assert_equal(res["all_networks"]["new"], 8)
        assert_equal(res["all_networks"]["tried"], 0)
        assert_equal(res["all_networks"]["total"], 8)

'''
    content = content[:insertion_point] + new_test + content[insertion_point:]

with open('test/functional/rpc_net.py', 'w') as f:
    f.write(content)

# Fix test_framework.py
with open('test/functional/test_framework/test_framework.py', 'r') as f:
    content = f.read()

# Find and replace the conflict
content = re.sub(r'<<<<<<< HEAD.*?>>>>>>> a945f09fa6.*?\n', '', content, flags=re.DOTALL)

# Add clear_addrman parameter to restart_node if not already there
if 'clear_addrman=False' not in content:
    # Find restart_node definition
    content = re.sub(r'(def restart_node\(self, i, extra_args=None)\):', 
                    r'\1def restart_node(self, i, extra_args=None, clear_addrman=False):', 
                    content)
    
    # Add addrman clearing logic
    content = re.sub(r'(self\.stop_node\(i\))\n(\s+)(self\.start_node\(i, extra_args\))',
                    r'\1\n\2if clear_addrman:\n\2    peers_dat = os.path.join(self.nodes[i].datadir, self.chain, "peers.dat")\n\2    os.remove(peers_dat) if os.path.isfile(peers_dat) else None\n\2    anchors_dat = os.path.join(self.nodes[i].datadir, self.chain, "anchors.dat")\n\2    os.remove(anchors_dat) if os.path.isfile(anchors_dat) else None\n\2\3',
                    content)

with open('test/functional/test_framework/test_framework.py', 'w') as f:
    f.write(content)

print("Test conflicts resolved")